#include "export/ExportOutputPlanner.hpp"
#include "export/ExportManager.hpp"
#include "export/ManualExportPlanner.hpp"
#include "export/FfmpegArgumentBuilder.hpp"
#include "export/ExportTypes.hpp"
#include "history/RecordingIndex.hpp"
#include "recording/OutputPathPolicy.hpp"
#include "ui/ManualExportViewModel.hpp"
#include "ui/SettingsViewModel.hpp"

#include <QCoreApplication>
#include <QDataStream>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

namespace {

int failures = 0;

void expect(const bool condition, const char* const message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        ++failures;
    }
}

abracapture::exporting::ExportRequest requestFor(
    const abracapture::exporting::ExportProfile& profile,
    const std::filesystem::path& destination = "/exports/track.mp3") {
    abracapture::exporting::ExportRequest request;
    request.sourcePath = "/masters/track.wav";
    request.destinationPath = destination;
    request.profile = profile;
    return request;
}

bool build(const abracapture::exporting::ExportRequest& request, QStringList* arguments) {
    QString error;
    const bool success = abracapture::exporting::FfmpegArgumentBuilder::build(
        request, arguments, &error);
    if (!success) {
        std::cerr << "Argument builder error: " << error.toStdString() << "\n";
    }
    return success;
}

bool writeSilenceWav(const std::filesystem::path& path) {
    constexpr quint32 sampleRate = 44100;
    constexpr quint16 channels = 1;
    constexpr quint16 bitsPerSample = 16;
    constexpr quint32 dataSize = sampleRate * channels * (bitsPerSample / 8);

    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + dataSize);
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32(16) << quint16(1) << channels << sampleRate;
    stream << quint32(sampleRate * channels * (bitsPerSample / 8));
    stream << quint16(channels * (bitsPerSample / 8)) << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataSize;
    stream.writeRawData(QByteArray(dataSize, '\0').constData(), dataSize);
    return stream.status() == QDataStream::Ok;
}

void testMp3Cbr320PreservesSourceRate() {
    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Mp3);
    QStringList arguments;
    expect(build(requestFor(profile), &arguments), "MP3 320 request builds");
    expect(arguments == QStringList{
                            QStringLiteral("-nostdin"), QStringLiteral("-n"),
                            QStringLiteral("-i"), QStringLiteral("/masters/track.wav"),
                            QStringLiteral("-c:a"), QStringLiteral("libmp3lame"),
                            QStringLiteral("-b:a"), QStringLiteral("320k"),
                            QStringLiteral("/exports/track.mp3")},
           "MP3 320 CBR arguments are exact and omit resampling");
    expect(!arguments.contains(QStringLiteral("-q:a")), "MP3 CBR does not use the old VBR quality flag");
}

void testMp3ExplicitSampleRate() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Mp3);
    profile.sampleRate = {abracapture::exporting::SampleRatePolicy::Explicit, 44100};
    QStringList arguments;
    expect(build(requestFor(profile), &arguments), "MP3 explicit-rate request builds");
    const qsizetype rateIndex = arguments.indexOf(QStringLiteral("-ar"));
    expect(rateIndex >= 0 && arguments.value(rateIndex + 1) == QStringLiteral("44100"),
           "MP3 explicit sample rate adds -ar 44100");
}

void testMp3RejectsUnsupportedPreserveSourceRate() {
    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Mp3);
    auto request = requestFor(profile);
    request.sourceSampleRate = 96000;
    QStringList arguments;
    QString error;
    expect(!abracapture::exporting::FfmpegArgumentBuilder::build(request, &arguments, &error),
           "MP3 preserve-source rejects an encoder-unsupported source rate");
    expect(error.contains(QStringLiteral("44.1 or 48")),
           "Unsupported MP3 preserve-source rate provides a useful correction");
}

void testFlacOptions() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Flac);
    profile.options = abracapture::exporting::FlacOptions{24, 8};
    QStringList arguments;
    expect(build(requestFor(profile, "/exports/track.flac"), &arguments), "FLAC request builds");
    expect(!arguments.contains(QStringLiteral("-ar")), "FLAC preserve-source omits resampling");
    expect(arguments.contains(QStringLiteral("-compression_level")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-compression_level")) + 1) == QStringLiteral("8"),
           "FLAC compression level is explicit");
    expect(arguments.contains(QStringLiteral("-bits_per_raw_sample")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-bits_per_raw_sample")) + 1) == QStringLiteral("24"),
           "FLAC 24-bit output is explicit");
    expect(arguments.contains(QStringLiteral("-af")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-af")) + 1).contains(
                   QStringLiteral("output_sample_bits=24:dither_method=triangular")),
           "FLAC 24-bit conversion uses explicit triangular dither");
}

void testFlac16ExplicitRateUsesDitheredResample() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Flac);
    profile.options = abracapture::exporting::FlacOptions{16, 5};
    profile.sampleRate = {abracapture::exporting::SampleRatePolicy::Explicit, 96000};
    QStringList arguments;
    expect(build(requestFor(profile, "/exports/track.flac"), &arguments),
           "FLAC 16-bit explicit-rate request builds");
    const qsizetype filterIndex = arguments.indexOf(QStringLiteral("-af"));
    expect(filterIndex >= 0 && arguments.value(filterIndex + 1).contains(
               QStringLiteral("out_sample_fmt=s16:output_sample_bits=16:dither_method=triangular:out_sample_rate=96000")),
           "FLAC 16-bit conversion combines dither and explicit resampling");
    expect(!arguments.contains(QStringLiteral("-ar")),
           "Dithered FLAC resampling does not add a competing -ar option");

    profile.sampleRate = {abracapture::exporting::SampleRatePolicy::PreserveSource, 0};
    profile.options = abracapture::exporting::FlacOptions{32, 5};
    expect(build(requestFor(profile, "/exports/track-32.flac"), &arguments),
           "FLAC 32-bit request builds through ffmpeg's supported s32 encoder path");
    expect(arguments.contains(QStringLiteral("-bits_per_raw_sample")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-bits_per_raw_sample")) + 1) == QStringLiteral("32"),
           "FLAC 32-bit output declares 32 raw sample bits");
    expect(!arguments.contains(QStringLiteral("-af")),
           "FLAC 32-bit output avoids unnecessary lower-bit dithering");
}

void testWavSampleFormat() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Wav);
    profile.options = abracapture::exporting::WavOptions{
        abracapture::exporting::WavSampleFormat::Signed24};
    QStringList arguments;
    expect(build(requestFor(profile, "/exports/track.wav"), &arguments), "WAV request builds");
    expect(arguments.contains(QStringLiteral("pcm_s24le")), "WAV signed 24-bit uses pcm_s24le");
    expect(arguments.contains(QStringLiteral("-af")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-af")) + 1).contains(
                   QStringLiteral("output_sample_bits=24:dither_method=triangular")),
           "WAV signed 24-bit conversion uses explicit triangular dither");

    profile.options = abracapture::exporting::WavOptions{
        abracapture::exporting::WavSampleFormat::Signed16};
    expect(build(requestFor(profile, "/exports/track-16.wav"), &arguments),
           "WAV signed 16-bit request builds");
    expect(arguments.contains(QStringLiteral("pcm_s16le")), "WAV signed 16-bit uses pcm_s16le");
    expect(arguments.contains(QStringLiteral("-af")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-af")) + 1).contains(
                   QStringLiteral("output_sample_bits=16:dither_method=triangular")),
           "WAV signed 16-bit conversion uses explicit triangular dither");

    profile.options = abracapture::exporting::WavOptions{
        abracapture::exporting::WavSampleFormat::Signed32};
    expect(build(requestFor(profile, "/exports/track-32.wav"), &arguments),
           "WAV signed 32-bit request builds");
    expect(arguments.contains(QStringLiteral("pcm_s32le")), "WAV signed 32-bit uses pcm_s32le");
    expect(!arguments.contains(QStringLiteral("-af")), "WAV signed 32-bit does not reduce precision");

    profile.options = abracapture::exporting::WavOptions{
        abracapture::exporting::WavSampleFormat::Float32};
    expect(build(requestFor(profile, "/exports/track-float.wav"), &arguments),
           "WAV float request builds");
    expect(arguments.contains(QStringLiteral("pcm_f32le")), "WAV float uses pcm_f32le");
    expect(!arguments.contains(QStringLiteral("-af")), "WAV float preserves float samples without dither");
}

void testMetadataNaming() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary metadata naming directory is available");
    if (!directory.isValid()) {
        return;
    }
    const std::filesystem::path root = directory.path().toStdString();
    abracapture::recording::OutputPathPolicy policy(root, "{source}_{date}_{time}", "wav");

    abracapture::recording::CaptureMetadata metadata;
    metadata.source = "Firefox";
    metadata.format = "wav";
    metadata.automatedCapture = true;
    metadata.useMediaMetadataNames = true;
    metadata.mediaTitle = "Battery";
    metadata.mediaAlbum = "Master of Puppets";
    const std::filesystem::path first = policy.resolveOutputPath(metadata);
    expect(first.parent_path().filename() == "Master of Puppets" && first.filename() == "Battery.wav",
           "Automated metadata capture uses album directory and track filename");

    const std::string sanitized = abracapture::recording::OutputPathPolicy::sanitizePathComponent(
        "../Battery/\\:*?\"<>|\n");
    expect(!sanitized.empty() && sanitized.find('/') == std::string::npos &&
               sanitized.find('\\') == std::string::npos && sanitized != "." && sanitized != "..",
           "Metadata path components are sanitized against traversal and reserved characters");

    abracapture::recording::CaptureMetadata noAlbum = metadata;
    noAlbum.mediaAlbum.clear();
    const std::filesystem::path noAlbumPath = policy.resolveOutputPath(noAlbum);
    expect(noAlbumPath.parent_path() == std::filesystem::absolute(root) &&
               noAlbumPath.filename() == "Battery.wav",
           "Missing album metadata keeps automated capture in configured root");

    abracapture::recording::CaptureMetadata noTrack = metadata;
    noTrack.mediaTitle.clear();
    noTrack.mediaName.clear();
    const std::filesystem::path noTrackPath = policy.resolveOutputPath(noTrack);
    expect(noTrackPath.parent_path().filename() == "Master of Puppets" &&
               noTrackPath.filename().string().starts_with("Firefox_"),
           "Missing track metadata falls back to the configured generated filename");

    QFile occupied(QString::fromStdString(first.string()));
    expect(occupied.open(QIODevice::WriteOnly), "Metadata collision fixture can be created");
    occupied.close();
    const std::filesystem::path repeated = policy.resolveOutputPath(metadata);
    expect(repeated.filename() == "Battery_001.wav",
           "Repeated metadata track names use a non-destructive numeric suffix");

    abracapture::recording::CaptureMetadata manual = metadata;
    manual.automatedCapture = false;
    const std::filesystem::path manualPath = policy.resolveOutputPath(manual);
    expect(manualPath.parent_path() == std::filesystem::absolute(root) &&
               manualPath.filename().string().starts_with("Firefox_"),
           "Manual capture does not move into metadata album folders");
}

void testInvalidProfileIsRejected() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Mp3);
    profile.options = abracapture::exporting::FlacOptions{};
    QString error;
    expect(!profile.validate(&error), "Mismatched format options are rejected");
    expect(!error.isEmpty(), "Invalid profile supplies a diagnostic");

    QStringList arguments;
    expect(!abracapture::exporting::FfmpegArgumentBuilder::build(
               requestFor(profile), &arguments, &error),
           "Argument builder rejects invalid profile");
}

void testOutputPlanningAvoidsCollisions() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary export directory is available");
    if (!directory.isValid()) {
        return;
    }

    const std::filesystem::path desired =
        std::filesystem::path(directory.path().toStdString()) / "track.mp3";
    QFile occupied(QString::fromStdString(desired.string()));
    expect(occupied.open(QIODevice::WriteOnly), "Collision fixture can be created");
    occupied.close();

    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Mp3);
    abracapture::exporting::ExportRequest planned;
    QString error;
    expect(abracapture::exporting::ExportOutputPlanner::plan(
               requestFor(profile, desired), {}, &planned, &error),
           "Collision planner accepts a unique-name policy");
    expect(planned.destinationPath.filename() == "track_001.mp3",
           "Collision planner selects the expected numeric suffix");

    auto failProfile = profile;
    failProfile.collisionPolicy = abracapture::exporting::CollisionPolicy::FailIfExists;
    expect(!abracapture::exporting::ExportOutputPlanner::plan(
               requestFor(failProfile, desired), {}, &planned, &error),
           "Fail-if-exists collision policy rejects occupied output");

    auto replaceProfile = profile;
    replaceProfile.collisionPolicy = abracapture::exporting::CollisionPolicy::ReplaceExisting;
    expect(abracapture::exporting::ExportOutputPlanner::plan(
               requestFor(replaceProfile, desired), {}, &planned, &error) &&
               planned.destinationPath == desired,
           "Replace-existing requires an explicit policy and retains the requested path");
}

void testManualProfileModesAndBulkPreflight() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Manual profile fixture directory is available");
    if (!directory.isValid()) return;
    const std::filesystem::path root = directory.path().toStdString();
    const auto first = root / "first.wav";
    const auto second = root / "second.wav";
    expect(writeSilenceWav(first) && writeSilenceWav(second), "Bulk profile WAV fixtures can be created");
    QFile existing(QString::fromStdString((root / "first.mp3").string()));
    expect(existing.open(QIODevice::WriteOnly), "Existing derivative fixture can be created");
    existing.write("existing", 8);
    existing.close();

    abracapture::exporting::ExportManager manager;
    SettingsViewModel settings;
    const auto automaticBefore = settings.automaticExportProfile().profile;
    ManualExportViewModel manual(&manager, nullptr, &settings);
    expect(manual.profileMode() == QStringLiteral("automatic"),
           "Manual export starts with an explicit Automatic Profile mode");
    expect(!manual.activateSelectedRecordings() && manual.sourceMode() == QStringLiteral("recordings") &&
               manual.sourceCount() == 0 && manual.statusMessage().contains(QStringLiteral("Recordings page")),
           "Empty Selected Recordings mode gives direct navigation guidance without stale sources");
    manual.setProfileMode(QStringLiteral("manual"));
    manual.setManualFormat(QStringLiteral("flac"));
    manual.setManualSampleRate(44100);
    manual.setManualFlacBitDepth(16);
    expect(manual.profileMode() == QStringLiteral("manual") &&
               manual.profileSummary().contains(QStringLiteral("FLAC")) &&
               manual.profileSummary().contains(QStringLiteral("16-bit")),
           "Custom manual FLAC profile is independent and visible");
    const auto* automaticMp3 = std::get_if<abracapture::exporting::Mp3Options>(&automaticBefore.options);
    const auto automaticAfter = settings.automaticExportProfile().profile;
    const auto* unchangedAutomaticMp3 = std::get_if<abracapture::exporting::Mp3Options>(
        &automaticAfter.options);
    expect(automaticMp3 != nullptr && unchangedAutomaticMp3 != nullptr &&
               automaticMp3->bitrateKbps == unchangedAutomaticMp3->bitrateKbps,
           "Changing the manual profile does not alter automatic export settings");
    manual.setManualFormat(QStringLiteral("wav"));
    manual.setManualWavSampleFormat(QStringLiteral("s24"));
    expect(manual.profileSummary().contains(QStringLiteral("WAV")) &&
               manual.profileSummary().contains(QStringLiteral("24-bit")),
           "Custom manual WAV profile is selectable");

    manual.setManualFormat(QStringLiteral("mp3"));
    manual.setManualMp3Bitrate(320);
    manual.setSelectedRecordings({
        QVariantMap{{QStringLiteral("filePath"), QString::fromStdString(first.string())}},
        QVariantMap{{QStringLiteral("filePath"), QString::fromStdString(second.string())}}
    });
    expect(manual.sourceCount() == 2, "Multiple recordings become one manual source set");
    expect(manual.collisionPolicy() == QStringLiteral("skip"),
           "Bulk manual export defaults to Skip Existing");
    const QVariantMap preview = manual.bulkPreview();
    expect(preview.value(QStringLiteral("skippedCount")).toInt() == 1 &&
               preview.value(QStringLiteral("newCount")).toInt() == 1,
           "Bulk preflight counts existing and new outputs before enqueue");
    expect(manual.enqueueExport() == 1 && manager.jobs().size() == 1,
           "Skipped existing output is not submitted to ffmpeg");
    manual.setCollisionPolicy(QStringLiteral("unique"));
    expect(manual.bulkPreview().value(QStringLiteral("newCount")).toInt() == 2,
           "Explicit unique-name policy plans both bulk outputs");
}

void testQueueAcceptsMultiplePendingJobs() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary queue directory is available");
    if (!directory.isValid()) {
        return;
    }

    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path sourcePath = root / "master.wav";
    QFile source(QString::fromStdString(sourcePath.string()));
    expect(source.open(QIODevice::WriteOnly), "Queue source fixture can be created");
    source.write("master", 6);
    source.close();

    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Mp3);
    auto first = requestFor(profile, root / "first.mp3");
    auto second = requestFor(profile, root / "first.mp3");
    first.sourcePath = sourcePath;
    second.sourcePath = sourcePath;

    abracapture::exporting::ExportManager manager;
    const QList<abracapture::exporting::ExportJobId> jobIds = manager.enqueueAll({first, second});
    expect(jobIds.size() == 2 && jobIds.at(0) != jobIds.at(1),
           "Queue assigns unique IDs to both submitted jobs");
    expect(manager.queuedJobCount() == 2 && manager.isBusy(),
           "Queue retains a second job instead of rejecting it");
    const auto firstJob = manager.job(jobIds.at(0));
    const auto secondJob = manager.job(jobIds.at(1));
    expect(firstJob.has_value() && secondJob.has_value() &&
               firstJob->state == abracapture::exporting::ExportJobState::Queued &&
               secondJob->state == abracapture::exporting::ExportJobState::Queued,
           "Both jobs remain queued until asynchronous processing starts");
    expect(firstJob.has_value() && secondJob.has_value() &&
               firstJob->request.destinationPath.filename() == "first.mp3" &&
               secondJob->request.destinationPath.filename() == "first_001.mp3",
           "Queued jobs reserve distinct collision-safe output paths");
}

void testManualDirectoryEnumerationAndOutputPlanning() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary manual-export directory is available");
    if (!directory.isValid()) return;

    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path rootMaster = root / "root.wav";
    const std::filesystem::path nestedDirectory = root / "nested";
    const std::filesystem::path nestedMaster = nestedDirectory / "nested.flac";
    const std::filesystem::path exportsDirectory = root / "Exports";
    const std::filesystem::path generatedExport = exportsDirectory / "root.flac";
    std::filesystem::create_directory(nestedDirectory);
    std::filesystem::create_directory(exportsDirectory);
    QFile rootFile{QString::fromStdString(rootMaster.string())};
    expect(rootFile.open(QIODevice::WriteOnly), "Root master fixture can be created");
    rootFile.write("master", 6);
    rootFile.close();
    QFile nestedFile(QString::fromStdString(nestedMaster.string()));
    expect(nestedFile.open(QIODevice::WriteOnly), "Nested master fixture can be created");
    nestedFile.write("master", 6);
    nestedFile.close();
    QFile generatedFile(QString::fromStdString(generatedExport.string()));
    expect(generatedFile.open(QIODevice::WriteOnly), "Generated export fixture can be created");
    generatedFile.write("export", 6);
    generatedFile.close();

    const auto nonRecursive = abracapture::exporting::ManualExportPlanner::enumerateDirectory(root, false);
    expect(nonRecursive.size() == 1 && nonRecursive.front().path.filename() == "root.wav",
           "Directory export is non-recursive by default");
    const auto recursive = abracapture::exporting::ManualExportPlanner::enumerateDirectory(root, true);
    expect(recursive.size() == 2, "Directory export includes nested masters only when explicitly enabled");
    bool includesGeneratedExport = false;
    for (const auto& candidate : recursive) {
        if (candidate.path == generatedExport) includesGeneratedExport = true;
    }
    expect(!includesGeneratedExport,
           "Exports subdirectory is excluded from recursive input enumeration");
    expect(abracapture::exporting::ManualExportPlanner::supportedInputExtensions() ==
               QStringList{QStringLiteral("wav"), QStringLiteral("flac")},
           "Manual directory inputs use the centralized master-format list");

    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Mp3);
    abracapture::exporting::ManualExportSource source;
    source.path = rootMaster;
    source.sampleRate = 44100;
    source.trackTitle = QStringLiteral("Battery");
    source.albumTitle = QStringLiteral("Master of Puppets");
    abracapture::exporting::ManualExportDestination destination;
    destination.policy = abracapture::exporting::ManualExportDestinationPolicy::ExportsSubfolder;
    destination.useStoredMetadataNames = true;
    destination.organizeAlbumDirectory = true;
    abracapture::exporting::ExportRequest request;
    QString error;
    expect(abracapture::exporting::ManualExportPlanner::planRequest(
               source, profile, destination, false, &request, &error),
           "Manual metadata export request plans without touching the filesystem");
    expect(request.destinationPath == root / "Exports" / "Master of Puppets" / "Battery.mp3",
           "Manual metadata export uses the explicit album organization policy");

    destination.useStoredMetadataNames = false;
    destination.organizeAlbumDirectory = false;
    expect(abracapture::exporting::ManualExportPlanner::planRequest(
               source, profile, destination, false, &request, &error),
           "Manual filename-preserving request plans");
    expect(request.destinationPath == root / "Exports" / "root.mp3",
           "Manual export preserves the source filename by default");
}

void testQueuedJobCanBeCancelled() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary cancellation directory is available");
    if (!directory.isValid()) return;
    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path sourcePath = root / "master.wav";
    QFile source(QString::fromStdString(sourcePath.string()));
    expect(source.open(QIODevice::WriteOnly), "Cancellation source fixture can be created");
    source.write("master", 6);
    source.close();
    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::ExportFormat::Mp3);
    auto request = requestFor(profile, root / "cancel.mp3");
    request.sourcePath = sourcePath;
    abracapture::exporting::ExportManager manager;
    const auto jobId = manager.enqueue(request);
    expect(manager.cancelQueued(jobId), "Queued export can be cancelled safely");
    const auto job = manager.job(jobId);
    expect(job.has_value() && job->state == abracapture::exporting::ExportJobState::Cancelled &&
               manager.queuedJobCount() == 0,
           "Cancelled export leaves the shared queue intact");
}

void testCaptureSessionResolutionAndRetry() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary manual session directory is available");
    if (!directory.isValid()) return;
    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path older = root / "older.wav";
    const std::filesystem::path recent = root / "recent.wav";
    QFile olderFile{QString::fromStdString(older.string())};
    QFile recentFile{QString::fromStdString(recent.string())};
    expect(olderFile.open(QIODevice::WriteOnly) && recentFile.open(QIODevice::WriteOnly),
           "Session master fixtures can be created");
    olderFile.write("master", 6); recentFile.write("master", 6);
    olderFile.close(); recentFile.close();

    abracapture::history::RecordingIndex index(QString::fromStdString((root / "history.json").string()));
    abracapture::history::RecordingEntry first;
    first.filePath = QString::fromStdString(older.string());
    first.captureSessionId = QStringLiteral("older-session");
    first.timestamp = QDateTime::currentDateTimeUtc().addSecs(-60);
    first.trackTitle = QStringLiteral("Older Track");
    abracapture::history::RecordingEntry second;
    second.filePath = QString::fromStdString(recent.string());
    second.captureSessionId = QStringLiteral("recent-session");
    second.timestamp = QDateTime::currentDateTimeUtc();
    second.trackTitle = QStringLiteral("Recent Track");
    second.albumTitle = QStringLiteral("Recent Album");
    expect(index.append(first) && index.append(second), "Session fixtures are indexed");

    abracapture::exporting::ExportManager manager;
    ManualExportViewModel manual(&manager, &index, nullptr);
    expect(manual.captureSessions().size() == 2, "Indexed sessions resolve by stable capture-session ID");
    expect(manual.selectMostRecentCaptureSession() && manual.sourceCount() == 1 &&
               manual.selectedSessionId() == QStringLiteral("recent-session"),
           "Most recent capture session resolves its own master without filename grouping");

    const std::filesystem::path invalidWav = root / "invalid.wav";
    QFile invalidFile{QString::fromStdString(invalidWav.string())};
    expect(invalidFile.open(QIODevice::WriteOnly), "Invalid audio retry fixture can be created");
    invalidFile.write("not audio", 9); invalidFile.close();
    expect(manual.selectFile(QString::fromStdString(invalidWav.string())), "Manual file selection accepts supported extension");
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&manager, &abracapture::exporting::ExportManager::jobFailed,
                     &loop, [&loop](const abracapture::exporting::ExportJobId, const QString&) { loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    expect(manual.enqueueExport() == 1, "Manual file export enters the shared queue");
    timeout.start(10000);
    loop.exec();
    const QVariantList jobs = manual.jobs();
    expect(!jobs.isEmpty() && jobs.constFirst().toMap().value(QStringLiteral("status")) == QStringLiteral("Failed"),
           "Failed manual export remains visible");
    const qulonglong failedId = jobs.constFirst().toMap().value(QStringLiteral("jobId")).toULongLong();
    expect(manual.retry(failedId) && manual.jobs().size() == 2,
           "Retry creates a new shared-queue job from the failed request");
}

void testManualFileWorkflowCompletes() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary manual-file directory is available");
    if (!directory.isValid()) return;
    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path sourcePath = root / "manual.wav";
    expect(writeSilenceWav(sourcePath), "Manual-file WAV fixture can be created");

    abracapture::exporting::ExportManager manager;
    ManualExportViewModel manual(&manager, nullptr, nullptr);
    expect(manual.selectFile(QString::fromStdString(sourcePath.string())),
           "Manual file mode accepts a supported master");
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    QObject::connect(&manager, &abracapture::exporting::ExportManager::jobCompleted,
                     &loop, [&completed, &loop](const abracapture::exporting::ExportJobId,
                                                 const QString&) { completed = true; loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    expect(manual.enqueueExport() == 1, "Manual file mode queues one shared export job");
    timeout.start(10000);
    loop.exec();
    const QVariantList jobs = manual.jobs();
    expect(completed && jobs.size() == 1 &&
               jobs.constFirst().toMap().value(QStringLiteral("status")) == QStringLiteral("Complete"),
           "Manual file job completes through the shared export manager");
    expect(QFileInfo::exists(QString::fromStdString(sourcePath.string())) &&
               QFileInfo::exists(QString::fromStdString((root / "manual.mp3").string())),
           "Manual file export creates a derivative without removing the master");
}

void testManagerGeneratesMp3320() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary ffmpeg directory is available");
    if (!directory.isValid()) {
        return;
    }

    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path sourcePath = root / "master.wav";
    const std::filesystem::path destinationPath = root / "master.mp3";
    expect(writeSilenceWav(sourcePath), "Synthetic WAV source can be created");
    if (!QFileInfo::exists(QString::fromStdString(sourcePath.string()))) {
        return;
    }

    auto request = requestFor(
        abracapture::exporting::defaultExportProfile(abracapture::exporting::ExportFormat::Mp3),
        destinationPath);
    request.sourcePath = sourcePath;

    abracapture::exporting::ExportManager manager;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    bool failed = false;
    QString outputPath;
    QObject::connect(&manager, &abracapture::exporting::ExportManager::jobCompleted,
                     &loop, [&](const abracapture::exporting::ExportJobId, const QString& output) {
                         completed = true;
                         outputPath = output;
                         loop.quit();
                     });
    QObject::connect(&manager, &abracapture::exporting::ExportManager::jobFailed,
                     &loop, [&](const abracapture::exporting::ExportJobId, const QString&) {
                         failed = true;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    const auto jobId = manager.enqueue(request);
    const auto queuedJob = manager.job(jobId);
    expect(queuedJob.has_value(), "Real export job is created");
    if (queuedJob.has_value() && queuedJob->state == abracapture::exporting::ExportJobState::Queued) {
        timeout.start(10000);
        loop.exec();
    }

    expect(completed && !failed, "ffmpeg completes the MP3 export");
    expect(QFileInfo(outputPath).exists() && QFileInfo(outputPath).size() > 0,
           "Completed MP3 output is non-empty");
    expect(QFileInfo::exists(QString::fromStdString(sourcePath.string())),
           "Master WAV remains intact after export");
    if (!completed) {
        return;
    }

    QProcess ffprobe;
    ffprobe.start(QStringLiteral("ffprobe"), {
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-show_entries"), QStringLiteral("stream=codec_name,sample_rate,bit_rate"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1"), outputPath
    });
    expect(ffprobe.waitForFinished(5000), "ffprobe inspects the generated MP3");
    const QString probeOutput = QString::fromLocal8Bit(ffprobe.readAllStandardOutput());
    expect(probeOutput.contains(QStringLiteral("codec_name=mp3")),
           "Generated export reports the MP3 codec");
    expect(probeOutput.contains(QStringLiteral("sample_rate=44100")),
           "Generated export preserves the source sample rate");
    expect(probeOutput.contains(QStringLiteral("bit_rate=320000")),
           "Generated export reports the requested 320 kbps bitrate");
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    testMp3Cbr320PreservesSourceRate();
    testMp3ExplicitSampleRate();
    testMp3RejectsUnsupportedPreserveSourceRate();
    testFlacOptions();
    testFlac16ExplicitRateUsesDitheredResample();
    testWavSampleFormat();
    testInvalidProfileIsRejected();
    testOutputPlanningAvoidsCollisions();
    testManualProfileModesAndBulkPreflight();
    testQueueAcceptsMultiplePendingJobs();
    testManualDirectoryEnumerationAndOutputPlanning();
    testQueuedJobCanBeCancelled();
    testCaptureSessionResolutionAndRetry();
    testManualFileWorkflowCompletes();
    testMetadataNaming();
    testManagerGeneratesMp3320();
    return failures == 0 ? 0 : 1;
}
