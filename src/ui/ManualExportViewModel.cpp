#include "ManualExportViewModel.hpp"

#include "SettingsViewModel.hpp"
#include "history/RecordingIndex.hpp"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include <sndfile.h>

namespace {

using abracapture::exporting::ExportFormat;
using abracapture::exporting::ExportJobId;
using abracapture::exporting::ExportJobState;
using abracapture::exporting::ExportProfile;
using abracapture::exporting::FlacOptions;
using abracapture::exporting::ManualExportDestinationPolicy;
using abracapture::exporting::Mp3Options;
using abracapture::exporting::SampleRatePolicy;
using abracapture::exporting::WavOptions;
using abracapture::exporting::WavSampleFormat;

QString wavSampleFormatName(const WavSampleFormat format) {
    switch (format) {
    case WavSampleFormat::Signed16: return QStringLiteral("s16");
    case WavSampleFormat::Signed24: return QStringLiteral("s24");
    case WavSampleFormat::Signed32: return QStringLiteral("s32");
    case WavSampleFormat::Float32: return QStringLiteral("f32");
    case WavSampleFormat::Unsigned8: return QStringLiteral("u8");
    }
    return QStringLiteral("s16");
}

std::optional<WavSampleFormat> wavSampleFormatFromName(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("s16")) return WavSampleFormat::Signed16;
    if (normalized == QStringLiteral("s24")) return WavSampleFormat::Signed24;
    if (normalized == QStringLiteral("s32")) return WavSampleFormat::Signed32;
    if (normalized == QStringLiteral("f32")) return WavSampleFormat::Float32;
    return std::nullopt;
}

QString destinationPolicyName(const ManualExportDestinationPolicy policy) {
    switch (policy) {
    case ManualExportDestinationPolicy::SameDirectory: return QStringLiteral("same");
    case ManualExportDestinationPolicy::ExportsSubfolder: return QStringLiteral("exports");
    case ManualExportDestinationPolicy::CustomDirectory: return QStringLiteral("custom");
    }
    return QStringLiteral("same");
}

ManualExportDestinationPolicy destinationPolicyFromName(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("exports")) return ManualExportDestinationPolicy::ExportsSubfolder;
    if (normalized == QStringLiteral("custom")) return ManualExportDestinationPolicy::CustomDirectory;
    return ManualExportDestinationPolicy::SameDirectory;
}

QString jobStateName(const ExportJobState state) {
    switch (state) {
    case ExportJobState::Queued: return QStringLiteral("Queued");
    case ExportJobState::Running: return QStringLiteral("Exporting");
    case ExportJobState::Completed: return QStringLiteral("Complete");
    case ExportJobState::Failed: return QStringLiteral("Failed");
    case ExportJobState::Cancelled: return QStringLiteral("Cancelled");
    }
    return QStringLiteral("Unknown");
}

QString localPath(const QString& value) {
    const QUrl url(value);
    return url.isLocalFile() ? url.toLocalFile() : value;
}

} // namespace

ManualExportViewModel::ManualExportViewModel(
    abracapture::exporting::ExportManager* const exportManager,
    abracapture::history::RecordingIndex* const recordingIndex,
    SettingsViewModel* const settingsViewModel,
    QObject* const parent)
    : QObject(parent)
    , exportManager_(exportManager)
    , recordingIndex_(recordingIndex)
    , settingsViewModel_(settingsViewModel)
    , manualProfile_(abracapture::exporting::defaultExportProfile(ExportFormat::Mp3)) {
    rebuildCaptureSessions();
    if (exportManager_ == nullptr) {
        updateStatus(QStringLiteral("Export manager is unavailable"));
        return;
    }
    connect(exportManager_, &abracapture::exporting::ExportManager::jobStarted, this,
            [this](const ExportJobId jobId) {
                if (!manualRequests_.contains(jobId)) return;
                updateDerivedExport(jobId, QStringLiteral("running"), {}, {});
                emit jobsChanged();
            });
    connect(exportManager_, &abracapture::exporting::ExportManager::jobCompleted, this,
            [this](const ExportJobId jobId, const QString& outputPath) {
                if (!manualRequests_.contains(jobId)) return;
                updateDerivedExport(jobId, QStringLiteral("completed"), outputPath, {});
                updateStatus(QStringLiteral("Completed %1").arg(QFileInfo(outputPath).fileName()));
                emit jobsChanged();
            });
    connect(exportManager_, &abracapture::exporting::ExportManager::jobFailed, this,
            [this](const ExportJobId jobId, const QString& error) {
                if (!manualRequests_.contains(jobId)) return;
                updateDerivedExport(jobId, QStringLiteral("failed"), {}, error);
                updateStatus(error);
                emit jobsChanged();
            });
    connect(exportManager_, &abracapture::exporting::ExportManager::jobCancelled, this,
            [this](const ExportJobId jobId) {
                if (!manualRequests_.contains(jobId)) return;
                updateDerivedExport(jobId, QStringLiteral("cancelled"), {}, {});
                emit jobsChanged();
            });
}

QString ManualExportViewModel::sourceMode() const { return sourceMode_; }
QString ManualExportViewModel::selectedFile() const { return selectedFile_; }
QString ManualExportViewModel::selectedDirectory() const { return selectedDirectory_; }
bool ManualExportViewModel::includeSubdirectories() const noexcept { return includeSubdirectories_; }

void ManualExportViewModel::setIncludeSubdirectories(const bool enabled) {
    if (includeSubdirectories_ == enabled) return;
    includeSubdirectories_ = enabled;
    if (sourceMode_ == QStringLiteral("directory")) refreshDirectory();
    else emit sourceChanged();
}

QVariantList ManualExportViewModel::sources() const {
    QVariantList result;
    result.reserve(sources_.size());
    for (const auto& source : sources_) result.append(sourceDetails(source));
    return result;
}

int ManualExportViewModel::sourceCount() const noexcept { return sources_.size(); }
QVariantList ManualExportViewModel::captureSessions() const { return captureSessions_; }
QString ManualExportViewModel::selectedSessionId() const { return selectedSessionId_; }
bool ManualExportViewModel::useAutomaticProfile() const noexcept { return useAutomaticProfile_; }

void ManualExportViewModel::setUseAutomaticProfile(const bool enabled) {
    if (useAutomaticProfile_ == enabled) return;
    useAutomaticProfile_ = enabled;
    emit profileChanged();
}

QString ManualExportViewModel::manualFormat() const {
    return abracapture::exporting::exportFormatName(manualProfile_.format);
}

void ManualExportViewModel::setManualFormat(const QString& format) {
    const auto parsed = abracapture::exporting::exportFormatFromString(format);
    if (!parsed.has_value() || *parsed == manualProfile_.format) return;
    manualProfile_ = abracapture::exporting::defaultExportProfile(*parsed);
    emit profileChanged();
}

int ManualExportViewModel::manualSampleRate() const noexcept {
    return manualProfile_.sampleRate.policy == SampleRatePolicy::Explicit
               ? static_cast<int>(manualProfile_.sampleRate.sampleRate) : 0;
}

void ManualExportViewModel::setManualSampleRate(const int rate) {
    if (rate == 0) {
        manualProfile_.sampleRate = {SampleRatePolicy::PreserveSource, 0};
    } else if (abracapture::exporting::supportsExplicitSampleRate(
                   manualProfile_.format, static_cast<std::uint32_t>(rate))) {
        manualProfile_.sampleRate = {SampleRatePolicy::Explicit, static_cast<std::uint32_t>(rate)};
    } else {
        return;
    }
    emit profileChanged();
}

int ManualExportViewModel::manualMp3Bitrate() const noexcept {
    if (const auto* options = std::get_if<Mp3Options>(&manualProfile_.options)) return options->bitrateKbps;
    return 320;
}

void ManualExportViewModel::setManualMp3Bitrate(const int bitrate) {
    if (manualProfile_.format != ExportFormat::Mp3 ||
        (bitrate != 128 && bitrate != 192 && bitrate != 256 && bitrate != 320)) return;
    manualProfile_.options = Mp3Options{static_cast<std::uint32_t>(bitrate),
                                         abracapture::exporting::Mp3BitrateMode::Constant};
    emit profileChanged();
}

int ManualExportViewModel::manualFlacBitDepth() const noexcept {
    if (const auto* options = std::get_if<FlacOptions>(&manualProfile_.options)) return options->bitDepth;
    return 16;
}

void ManualExportViewModel::setManualFlacBitDepth(const int bitDepth) {
    if (manualProfile_.format != ExportFormat::Flac ||
        (bitDepth != 16 && bitDepth != 24 && bitDepth != 32)) return;
    const auto* options = std::get_if<FlacOptions>(&manualProfile_.options);
    manualProfile_.options = FlacOptions{static_cast<std::uint32_t>(bitDepth),
                                          options != nullptr ? options->compressionLevel : 5};
    emit profileChanged();
}

int ManualExportViewModel::manualFlacCompressionLevel() const noexcept {
    if (const auto* options = std::get_if<FlacOptions>(&manualProfile_.options)) return options->compressionLevel;
    return 5;
}

void ManualExportViewModel::setManualFlacCompressionLevel(const int level) {
    if (manualProfile_.format != ExportFormat::Flac || level < 0 || level > 12) return;
    const auto* options = std::get_if<FlacOptions>(&manualProfile_.options);
    manualProfile_.options = FlacOptions{options != nullptr ? options->bitDepth : 16, level};
    emit profileChanged();
}

QString ManualExportViewModel::manualWavSampleFormat() const {
    if (const auto* options = std::get_if<WavOptions>(&manualProfile_.options)) {
        return wavSampleFormatName(options->sampleFormat);
    }
    return QStringLiteral("s16");
}

QString ManualExportViewModel::manualPreset() const {
    const auto& profile = manualProfile_;
    if (profile.sampleRate.policy != SampleRatePolicy::PreserveSource) return QStringLiteral("custom");
    if (profile.format == ExportFormat::Mp3) {
        const auto* options = std::get_if<Mp3Options>(&profile.options);
        if (options != nullptr && options->bitrateMode == abracapture::exporting::Mp3BitrateMode::Constant) {
            if (options->bitrateKbps == 320) return QStringLiteral("mp3_320");
            if (options->bitrateKbps == 256) return QStringLiteral("mp3_256");
        }
    } else if (profile.format == ExportFormat::Flac) {
        const auto* options = std::get_if<FlacOptions>(&profile.options);
        if (options != nullptr && options->compressionLevel == 5) {
            if (options->bitDepth == 16) return QStringLiteral("flac_16");
            if (options->bitDepth == 24) return QStringLiteral("flac_24");
        }
    } else if (profile.format == ExportFormat::Wav) {
        const auto* options = std::get_if<WavOptions>(&profile.options);
        if (options != nullptr && options->sampleFormat == WavSampleFormat::Signed24) return QStringLiteral("wav_24");
        if (options != nullptr && options->sampleFormat == WavSampleFormat::Float32) return QStringLiteral("preserve_lossless");
    }
    return QStringLiteral("custom");
}

void ManualExportViewModel::setManualPreset(const QString& preset) {
    const QString normalized = preset.trimmed().toLower();
    if (normalized == QStringLiteral("custom")) return;
    if (normalized == QStringLiteral("mp3_320") || normalized == QStringLiteral("mp3_256")) {
        manualProfile_ = abracapture::exporting::defaultExportProfile(ExportFormat::Mp3);
        std::get<Mp3Options>(manualProfile_.options).bitrateKbps = normalized == QStringLiteral("mp3_256") ? 256 : 320;
    } else if (normalized == QStringLiteral("flac_16") || normalized == QStringLiteral("flac_24")) {
        manualProfile_ = abracapture::exporting::defaultExportProfile(ExportFormat::Flac);
        std::get<FlacOptions>(manualProfile_.options).bitDepth = normalized == QStringLiteral("flac_24") ? 24 : 16;
    } else if (normalized == QStringLiteral("wav_24") || normalized == QStringLiteral("preserve_lossless")) {
        manualProfile_ = abracapture::exporting::defaultExportProfile(ExportFormat::Wav);
        std::get<WavOptions>(manualProfile_.options).sampleFormat = normalized == QStringLiteral("wav_24")
            ? WavSampleFormat::Signed24 : WavSampleFormat::Float32;
    } else return;
    emit profileChanged();
}

QStringList ManualExportViewModel::presetOptions() const {
    return {QStringLiteral("mp3_320"), QStringLiteral("mp3_256"), QStringLiteral("flac_16"),
            QStringLiteral("flac_24"), QStringLiteral("wav_24"),
            QStringLiteral("preserve_lossless"), QStringLiteral("custom")};
}

void ManualExportViewModel::setManualWavSampleFormat(const QString& format) {
    if (manualProfile_.format != ExportFormat::Wav) return;
    const auto parsed = wavSampleFormatFromName(format);
    if (!parsed.has_value()) return;
    manualProfile_.options = WavOptions{*parsed};
    emit profileChanged();
}

QStringList ManualExportViewModel::formatOptions() const {
    return {QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("wav")};
}
QStringList ManualExportViewModel::mp3BitrateOptions() const {
    return {QStringLiteral("128"), QStringLiteral("192"), QStringLiteral("256"), QStringLiteral("320")};
}
QStringList ManualExportViewModel::flacBitDepthOptions() const {
    return {QStringLiteral("16"), QStringLiteral("24"), QStringLiteral("32")};
}
QStringList ManualExportViewModel::flacCompressionOptions() const {
    QStringList options; for (int i = 0; i <= 12; ++i) options.append(QString::number(i)); return options;
}
QStringList ManualExportViewModel::wavSampleFormatOptions() const {
    return {QStringLiteral("s16"), QStringLiteral("s24"), QStringLiteral("s32"), QStringLiteral("f32")};
}

QStringList ManualExportViewModel::sampleRateOptions() const {
    const ExportProfile profile = activeProfile();
    if (profile.format == ExportFormat::Mp3) {
        return {QStringLiteral("preserve"), QStringLiteral("44100"), QStringLiteral("48000")};
    }
    return {QStringLiteral("preserve"), QStringLiteral("44100"), QStringLiteral("48000"),
            QStringLiteral("88200"), QStringLiteral("96000"), QStringLiteral("176400"),
            QStringLiteral("192000")};
}

QString ManualExportViewModel::profileSummary() const {
    return abracapture::exporting::ManualExportPlanner::profileSummary(activeProfile());
}

QString ManualExportViewModel::destinationPolicy() const { return destinationPolicyName(destination_.policy); }
void ManualExportViewModel::setDestinationPolicy(const QString& policy) {
    const auto next = destinationPolicyFromName(policy);
    if (destination_.policy == next) return;
    destination_.policy = next;
    emit destinationChanged();
}
QString ManualExportViewModel::customDestinationDirectory() const { return destination_.customDirectory; }
void ManualExportViewModel::setCustomDestinationDirectory(const QString& directory) {
    const QString next = QDir(directory).absolutePath();
    if (destination_.customDirectory == next) return;
    destination_.customDirectory = next;
    emit destinationChanged();
}
bool ManualExportViewModel::useStoredMetadataNames() const noexcept { return destination_.useStoredMetadataNames; }
void ManualExportViewModel::setUseStoredMetadataNames(const bool enabled) {
    if (destination_.useStoredMetadataNames == enabled) return;
    destination_.useStoredMetadataNames = enabled;
    emit destinationChanged();
}
bool ManualExportViewModel::organizeAlbumDirectory() const noexcept { return destination_.organizeAlbumDirectory; }
void ManualExportViewModel::setOrganizeAlbumDirectory(const bool enabled) {
    if (destination_.organizeAlbumDirectory == enabled) return;
    destination_.organizeAlbumDirectory = enabled;
    emit destinationChanged();
}

QString ManualExportViewModel::destinationPreview() const {
    if (sources_.isEmpty()) return QStringLiteral("Choose one or more source files to preview the destination.");
    abracapture::exporting::ExportRequest request;
    QString error;
    if (!abracapture::exporting::ManualExportPlanner::planRequest(
            sources_.constFirst(), activeProfile(), activeDestination(), false, &request, &error)) {
        return error;
    }
    return QDir::toNativeSeparators(QString::fromStdString(request.destinationPath.string()));
}

QVariantList ManualExportViewModel::jobs() const {
    QVariantList result;
    if (exportManager_ == nullptr) return result;
    for (const ExportJobId jobId : manualJobOrder_) {
        const auto job = exportManager_->job(jobId);
        if (!job.has_value()) continue;
        result.append(QVariantMap{
            {QStringLiteral("jobId"), QVariant::fromValue<qulonglong>(job->id)},
            {QStringLiteral("source"), QString::fromStdString(job->request.sourcePath.string())},
            {QStringLiteral("output"), QString::fromStdString((job->resultPath.empty() ? job->request.destinationPath : job->resultPath).string())},
            {QStringLiteral("profile"), abracapture::exporting::ManualExportPlanner::profileSummary(job->request.profile)},
            {QStringLiteral("status"), jobStateName(job->state)},
            {QStringLiteral("error"), job->error},
            {QStringLiteral("canRetry"), job->state == ExportJobState::Failed},
            {QStringLiteral("canCancel"), job->state == ExportJobState::Queued}
        });
    }
    return result;
}

QString ManualExportViewModel::statusMessage() const { return statusMessage_; }

bool ManualExportViewModel::selectFile(const QString& pathOrUrl) {
    const QString path = localPath(pathOrUrl);
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() ||
        !abracapture::exporting::ManualExportPlanner::isSupportedInput(path.toStdString())) {
        updateStatus(QStringLiteral("Choose a supported WAV or FLAC input file"));
        return false;
    }
    selectedFile_ = info.absoluteFilePath();
    selectedDirectory_.clear();
    selectedSessionId_.clear();
    setSources(QStringLiteral("file"), {sourceFromVariant({{QStringLiteral("filePath"), selectedFile_}})});
    return true;
}

bool ManualExportViewModel::selectDirectory(const QString& directory) {
    const QFileInfo info(localPath(directory));
    if (!info.exists() || !info.isDir()) {
        updateStatus(QStringLiteral("Choose an existing directory"));
        return false;
    }
    selectedDirectory_ = info.absoluteFilePath();
    selectedFile_.clear();
    selectedSessionId_.clear();
    refreshDirectory();
    return true;
}

void ManualExportViewModel::refreshDirectory() {
    if (selectedDirectory_.isEmpty()) return;
    QList<std::filesystem::path> exclusions;
    if (destination_.policy == ManualExportDestinationPolicy::CustomDirectory &&
        !destination_.customDirectory.isEmpty()) {
        exclusions.append(destination_.customDirectory.toStdString());
    }
    setSources(QStringLiteral("directory"), abracapture::exporting::ManualExportPlanner::enumerateDirectory(
        selectedDirectory_.toStdString(), includeSubdirectories_, exclusions));
    updateStatus(QStringLiteral("Found %1 eligible master file%2").arg(sources_.size())
                     .arg(sources_.size() == 1 ? QString() : QStringLiteral("s")));
}

void ManualExportViewModel::setSelectedRecordings(const QVariantList& entries) {
    QList<abracapture::exporting::ManualExportSource> next;
    for (const QVariant& value : entries) {
        if (!value.canConvert<QVariantMap>()) continue;
        const auto source = sourceFromVariant(value.toMap());
        if (!source.path.empty()) next.append(source);
    }
    selectedRecordingSources_ = next;
    selectedFile_.clear();
    selectedDirectory_.clear();
    selectedSessionId_.clear();
    setSources(QStringLiteral("recordings"), std::move(next));
}

bool ManualExportViewModel::activateSelectedRecordings() {
    if (selectedRecordingSources_.isEmpty()) {
        updateStatus(QStringLiteral("Select one or more masters on the Recordings page first"));
        return false;
    }
    selectedFile_.clear();
    selectedDirectory_.clear();
    selectedSessionId_.clear();
    setSources(QStringLiteral("recordings"), selectedRecordingSources_);
    return true;
}

bool ManualExportViewModel::selectMostRecentCaptureSession() {
    rebuildCaptureSessions();
    if (captureSessions_.isEmpty()) {
        updateStatus(QStringLiteral("No capture sessions with indexed masters are available"));
        return false;
    }
    return selectCaptureSession(captureSessions_.constFirst().toMap().value(QStringLiteral("sessionId")).toString());
}

void ManualExportViewModel::refreshCaptureSessions() {
    rebuildCaptureSessions();
}

bool ManualExportViewModel::selectCaptureSession(const QString& sessionId) {
    const auto it = sessionSources_.constFind(sessionId);
    if (it == sessionSources_.cend()) return false;
    selectedSessionId_ = sessionId;
    selectedFile_.clear();
    selectedDirectory_.clear();
    setSources(QStringLiteral("session"), it.value());
    return true;
}

int ManualExportViewModel::enqueueExport() {
    if (exportManager_ == nullptr || sources_.isEmpty()) {
        updateStatus(QStringLiteral("Choose at least one source before exporting"));
        return 0;
    }
    const ExportProfile profile = activeProfile();
    QList<abracapture::exporting::ExportRequest> requests;
    for (const auto& source : sources_) {
        abracapture::exporting::ExportRequest request;
        QString error;
        if (!abracapture::exporting::ManualExportPlanner::planRequest(
                source, profile, activeDestination(), true, &request, &error)) {
            updateStatus(error);
            continue;
        }
        requests.append(request);
    }
    if (requests.isEmpty()) return 0;
    const QList<ExportJobId> ids = exportManager_->enqueueAll(requests);
    for (const ExportJobId id : ids) {
        const auto job = exportManager_->job(id);
        if (!job.has_value()) continue;
        manualRequests_.insert(id, job->request);
        manualJobOrder_.append(id);
        if (recordingIndex_ != nullptr) recordingPathsByJob_.insert(id, QString::fromStdString(job->request.sourcePath.string()));
        updateDerivedExport(id, job->state == ExportJobState::Failed ? QStringLiteral("failed") : QStringLiteral("queued"),
                            {}, job->error);
    }
    updateStatus(QStringLiteral("Queued %1 export job%2").arg(ids.size())
                     .arg(ids.size() == 1 ? QString() : QStringLiteral("s")));
    emit jobsChanged();
    return ids.size();
}

bool ManualExportViewModel::retry(const qulonglong jobId) {
    if (exportManager_ == nullptr || !manualRequests_.contains(jobId)) return false;
    const auto prior = exportManager_->job(jobId);
    if (!prior.has_value() || prior->state != ExportJobState::Failed) return false;
    const ExportJobId retryId = exportManager_->enqueue(manualRequests_.value(jobId));
    const auto retryJob = exportManager_->job(retryId);
    if (!retryJob.has_value()) return false;
    manualRequests_.insert(retryId, retryJob->request);
    manualJobOrder_.append(retryId);
    if (recordingPathsByJob_.contains(jobId)) recordingPathsByJob_.insert(retryId, recordingPathsByJob_.value(jobId));
    updateDerivedExport(retryId, retryJob->state == ExportJobState::Failed ? QStringLiteral("failed") : QStringLiteral("queued"),
                        {}, retryJob->error);
    updateStatus(QStringLiteral("Retry queued"));
    emit jobsChanged();
    return true;
}

bool ManualExportViewModel::cancelQueued(const qulonglong jobId) {
    if (exportManager_ == nullptr || !manualRequests_.contains(jobId)) return false;
    return exportManager_->cancelQueued(jobId);
}

ExportProfile ManualExportViewModel::activeProfile() const {
    if (useAutomaticProfile_ && settingsViewModel_ != nullptr) {
        return settingsViewModel_->automaticExportProfile().profile;
    }
    return manualProfile_;
}

abracapture::exporting::ManualExportDestination ManualExportViewModel::activeDestination() const {
    return destination_;
}

abracapture::exporting::ManualExportSource ManualExportViewModel::sourceFromVariant(
    const QVariantMap& entry) const {
    abracapture::exporting::ManualExportSource source;
    source.path = entry.value(QStringLiteral("filePath")).toString().toStdString();
    source.sampleRate = entry.value(QStringLiteral("sampleRate")).toUInt();
    source.captureSessionId = entry.value(QStringLiteral("captureSessionId")).toString();
    source.trackTitle = entry.value(QStringLiteral("trackTitle")).toString();
    source.albumTitle = entry.value(QStringLiteral("albumTitle")).toString();
    return source;
}

QVariantMap ManualExportViewModel::sourceDetails(
    const abracapture::exporting::ManualExportSource& source) const {
    const QFileInfo info(QString::fromStdString(source.path.string()));
    int sampleRate = static_cast<int>(source.sampleRate);
    int channels = 0;
    int bitDepth = 0;
    QString sampleFormat;
    QString codec = info.suffix().toUpper();
    SF_INFO audioInfo{};
    SNDFILE* const audio = sf_open(info.absoluteFilePath().toUtf8().constData(), SFM_READ, &audioInfo);
    if (audio != nullptr) {
        if (sampleRate == 0) sampleRate = audioInfo.samplerate;
        channels = audioInfo.channels;
        const int subFormat = audioInfo.format & SF_FORMAT_SUBMASK;
        if (subFormat == SF_FORMAT_PCM_16) { bitDepth = 16; sampleFormat = QStringLiteral("16-bit PCM"); }
        else if (subFormat == SF_FORMAT_PCM_24) { bitDepth = 24; sampleFormat = QStringLiteral("24-bit PCM"); }
        else if (subFormat == SF_FORMAT_PCM_32) { bitDepth = 32; sampleFormat = QStringLiteral("32-bit PCM"); }
        else if (subFormat == SF_FORMAT_FLOAT) { bitDepth = 32; sampleFormat = QStringLiteral("32-bit Float"); }
        sf_close(audio);
    }
    return {
        {QStringLiteral("filePath"), info.absoluteFilePath()},
        {QStringLiteral("fileName"), info.fileName()},
        {QStringLiteral("codec"), codec},
        {QStringLiteral("sampleRate"), sampleRate},
        {QStringLiteral("channels"), channels},
        {QStringLiteral("bitDepth"), bitDepth},
        {QStringLiteral("sampleFormat"), sampleFormat},
        {QStringLiteral("trackTitle"), source.trackTitle},
        {QStringLiteral("albumTitle"), source.albumTitle}
    };
}

void ManualExportViewModel::setSources(QString mode,
                                       QList<abracapture::exporting::ManualExportSource> sources) {
    sourceMode_ = std::move(mode);
    sources_ = std::move(sources);
    emit sourceChanged();
    emit destinationChanged();
}

void ManualExportViewModel::rebuildCaptureSessions() {
    sessionSources_.clear();
    captureSessions_.clear();
    if (recordingIndex_ == nullptr) return;
    const auto entries = recordingIndex_->entries();
    QHash<QString, QDateTime> latestTimes;
    QHash<QString, QString> albumTitles;
    for (const auto& entry : entries) {
        if (entry.captureSessionId.isEmpty() || entry.filePath.isEmpty()) continue;
        const QFileInfo file(entry.filePath);
        if (!file.exists() || !file.isFile()) continue;
        auto source = sourceFromVariant({
            {QStringLiteral("filePath"), entry.filePath}, {QStringLiteral("sampleRate"), entry.sampleRate},
            {QStringLiteral("captureSessionId"), entry.captureSessionId}, {QStringLiteral("trackTitle"), entry.trackTitle},
            {QStringLiteral("albumTitle"), entry.albumTitle}});
        sessionSources_[entry.captureSessionId].append(source);
        if (!latestTimes.contains(entry.captureSessionId) || latestTimes.value(entry.captureSessionId) < entry.timestamp) {
            latestTimes.insert(entry.captureSessionId, entry.timestamp);
        }
        if (!entry.albumTitle.isEmpty()) albumTitles.insert(entry.captureSessionId, entry.albumTitle);
    }
    QList<QString> ids = sessionSources_.keys();
    std::sort(ids.begin(), ids.end(), [&latestTimes](const QString& left, const QString& right) {
        return latestTimes.value(left) > latestTimes.value(right);
    });
    for (const QString& id : ids) {
        const auto& sources = sessionSources_.value(id);
        QStringList tracks;
        for (const auto& source : sources) if (!source.trackTitle.isEmpty()) tracks.append(source.trackTitle);
        captureSessions_.append(QVariantMap{
            {QStringLiteral("sessionId"), id}, {QStringLiteral("timestamp"), latestTimes.value(id)},
            {QStringLiteral("count"), sources.size()}, {QStringLiteral("albumTitle"), albumTitles.value(id)},
            {QStringLiteral("tracks"), tracks.join(QStringLiteral(", "))}
        });
    }
    emit captureSessionsChanged();
}

void ManualExportViewModel::updateDerivedExport(const ExportJobId jobId, const QString& state,
                                                const QString& outputPath, const QString& error) {
    if (recordingIndex_ == nullptr || !recordingPathsByJob_.contains(jobId)) return;
    const QString sourcePath = recordingPathsByJob_.value(jobId);
    const auto entries = recordingIndex_->entries();
    for (auto entry : entries) {
        if (QDir::cleanPath(entry.filePath) != QDir::cleanPath(sourcePath)) continue;
        auto found = false;
        for (auto& derived : entry.derivedExports) {
            if (derived.jobId != jobId) continue;
            derived.state = state;
            if (!outputPath.isEmpty()) derived.outputPath = outputPath;
            derived.error = error;
            found = true;
            break;
        }
        if (!found) {
            const auto request = manualRequests_.value(jobId);
            entry.derivedExports.append({jobId,
                abracapture::exporting::exportFormatName(request.profile.format),
                outputPath.isEmpty() ? QString::fromStdString(request.destinationPath.string()) : outputPath,
                state, error});
        }
        recordingIndex_->update(entry);
        emit recordingHistoryChanged();
        break;
    }
}

void ManualExportViewModel::updateStatus(QString message) {
    if (statusMessage_ == message) return;
    statusMessage_ = std::move(message);
    emit statusMessageChanged();
}
