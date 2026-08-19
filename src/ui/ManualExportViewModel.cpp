#include "ManualExportViewModel.hpp"

#include "SettingsViewModel.hpp"
#include "export/AudioFormatCatalog.hpp"
#include "export/ExportOutputPlanner.hpp"
#include "history/RecordingIndex.hpp"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include <sndfile.h>

namespace {

using abracapture::exporting::AudioCodec;
using abracapture::exporting::AudioContainer;
using abracapture::exporting::AudioSampleFormat;
using abracapture::exporting::CodecOptions;
using abracapture::exporting::ExportJobId;
using abracapture::exporting::ExportJobState;
using abracapture::exporting::ExportProfile;
using abracapture::exporting::FlacOptions;
using abracapture::exporting::ManualExportDestinationPolicy;
using abracapture::exporting::Mp3Options;
using abracapture::exporting::PcmOptions;
using abracapture::exporting::SampleRatePolicy;
using abracapture::exporting::VorbisOptions;

QString destinationPolicyName(const ManualExportDestinationPolicy policy) {
    switch (policy) {
    case ManualExportDestinationPolicy::SameDirectory: return QStringLiteral("same");
    case ManualExportDestinationPolicy::ExportsSubfolder: return QStringLiteral("exports");
    case ManualExportDestinationPolicy::CustomDirectory: return QStringLiteral("custom");
    }
    return QStringLiteral("same");
}

QString collisionPolicyName(const abracapture::exporting::CollisionPolicy policy) {
    switch (policy) {
    case abracapture::exporting::CollisionPolicy::FailIfExists: return QStringLiteral("skip");
    case abracapture::exporting::CollisionPolicy::ReplaceExisting: return QStringLiteral("replace");
    case abracapture::exporting::CollisionPolicy::GenerateUniqueName: return QStringLiteral("unique");
    }
    return QStringLiteral("unique");
}

abracapture::exporting::CollisionPolicy collisionPolicyFromName(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("skip")) return abracapture::exporting::CollisionPolicy::FailIfExists;
    if (normalized == QStringLiteral("replace")) return abracapture::exporting::CollisionPolicy::ReplaceExisting;
    return abracapture::exporting::CollisionPolicy::GenerateUniqueName;
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
    , manualProfile_(abracapture::exporting::defaultExportProfile(AudioContainer::Mp3, AudioCodec::Mp3)) {
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
    emit bulkPreviewChanged();
}

QString ManualExportViewModel::profileMode() const {
    return useAutomaticProfile_ ? QStringLiteral("automatic") : QStringLiteral("manual");
}

void ManualExportViewModel::setProfileMode(const QString& mode) {
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("automatic")) setUseAutomaticProfile(true);
    else if (normalized == QStringLiteral("manual")) setUseAutomaticProfile(false);
}

QString ManualExportViewModel::manualFormat() const {
    return abracapture::exporting::audioContainerName(manualProfile_.target.container);
}

void ManualExportViewModel::setManualFormat(const QString& format) {
    const auto targetDesc = abracapture::exporting::AudioFormatCatalog::findTarget(format);
    if (!targetDesc.has_value()) return;
    if (manualProfile_.target.container == targetDesc->container &&
        manualProfile_.target.codec == targetDesc->codec) return;
    manualProfile_ = abracapture::exporting::defaultExportProfile(targetDesc->container, targetDesc->codec);
    emit profileChanged();
    emit bulkPreviewChanged();
}

int ManualExportViewModel::manualSampleRate() const noexcept {
    return manualProfile_.target.sampleRate.policy == SampleRatePolicy::Explicit
               ? static_cast<int>(manualProfile_.target.sampleRate.sampleRate) : 0;
}

void ManualExportViewModel::setManualSampleRate(const int rate) {
    if (rate == 0) {
        manualProfile_.target.sampleRate = {SampleRatePolicy::PreserveSource, 0};
    } else if (abracapture::exporting::supportsExplicitSampleRate(
                   manualProfile_.target.container, manualProfile_.target.codec,
                   static_cast<std::uint32_t>(rate))) {
        manualProfile_.target.sampleRate = {SampleRatePolicy::Explicit, static_cast<std::uint32_t>(rate)};
    } else {
        return;
    }
    emit profileChanged();
    emit bulkPreviewChanged();
}

int ManualExportViewModel::manualMp3Bitrate() const noexcept {
    if (const auto* options = std::get_if<Mp3Options>(&manualProfile_.target.codecOptions))
        return options->bitrateKbps;
    return 320;
}

void ManualExportViewModel::setManualMp3Bitrate(const int bitrate) {
    if (manualProfile_.target.codec != AudioCodec::Mp3 ||
        (bitrate != 128 && bitrate != 192 && bitrate != 256 && bitrate != 320)) return;
    manualProfile_.target.codecOptions = Mp3Options{static_cast<std::uint32_t>(bitrate),
                                                     abracapture::exporting::Mp3BitrateMode::Constant};
    emit profileChanged();
    emit bulkPreviewChanged();
}

int ManualExportViewModel::manualFlacBitDepth() const noexcept {
    if (const auto* options = std::get_if<FlacOptions>(&manualProfile_.target.codecOptions))
        return options->bitDepth;
    return 16;
}

void ManualExportViewModel::setManualFlacBitDepth(const int bitDepth) {
    if (manualProfile_.target.codec != AudioCodec::Flac ||
        (bitDepth != 16 && bitDepth != 24 && bitDepth != 32)) return;
    const auto* options = std::get_if<FlacOptions>(&manualProfile_.target.codecOptions);
    manualProfile_.target.codecOptions = FlacOptions{static_cast<std::uint32_t>(bitDepth),
                                                      options != nullptr ? options->compressionLevel : 5};
    emit profileChanged();
    emit bulkPreviewChanged();
}

int ManualExportViewModel::manualFlacCompressionLevel() const noexcept {
    if (const auto* options = std::get_if<FlacOptions>(&manualProfile_.target.codecOptions))
        return options->compressionLevel;
    return 5;
}

void ManualExportViewModel::setManualFlacCompressionLevel(const int level) {
    if (manualProfile_.target.codec != AudioCodec::Flac || level < 0 || level > 12) return;
    const auto* options = std::get_if<FlacOptions>(&manualProfile_.target.codecOptions);
    manualProfile_.target.codecOptions = FlacOptions{options != nullptr ? options->bitDepth : 16, level};
    emit profileChanged();
    emit bulkPreviewChanged();
}

QString ManualExportViewModel::manualWavSampleFormat() const {
    if (const auto* options = std::get_if<PcmOptions>(&manualProfile_.target.codecOptions)) {
        return abracapture::exporting::audioSampleFormatName(options->sampleFormat);
    }
    return QStringLiteral("s16");
}

QString ManualExportViewModel::manualPreset() const {
    return abracapture::exporting::AudioFormatCatalog::findMatchingPreset(manualProfile_.target);
}

void ManualExportViewModel::setManualPreset(const QString& preset) {
    const auto target = abracapture::exporting::AudioFormatCatalog::targetFromPreset(preset);
    if (!target.has_value()) return;
    manualProfile_.target = *target;
    emit profileChanged();
    emit bulkPreviewChanged();
}

QStringList ManualExportViewModel::presetOptions() const {
    return abracapture::exporting::AudioFormatCatalog::presetOptions();
}

QStringList ManualExportViewModel::presetLabels() const {
    return abracapture::exporting::AudioFormatCatalog::presetOptionLabels();
}

void ManualExportViewModel::setManualWavSampleFormat(const QString& format) {
    if (manualProfile_.target.codec != AudioCodec::Pcm) return;
    const auto parsed = abracapture::exporting::audioSampleFormatFromString(format);
    if (!parsed.has_value()) return;
    manualProfile_.target.codecOptions = PcmOptions{*parsed};
    emit profileChanged();
    emit bulkPreviewChanged();
}

QStringList ManualExportViewModel::formatOptions() const {
    QStringList formats;
    for (const auto& descriptor : abracapture::exporting::AudioFormatCatalog::targets()) {
        formats.append(abracapture::exporting::audioContainerName(descriptor.container));
    }
    return formats;
}

QStringList ManualExportViewModel::mp3BitrateOptions() const {
    return abracapture::exporting::AudioFormatCatalog::bitrateOptions(
        AudioContainer::Mp3, AudioCodec::Mp3);
}

QStringList ManualExportViewModel::flacBitDepthOptions() const {
    return abracapture::exporting::AudioFormatCatalog::bitDepthOptions(
        AudioContainer::Flac, AudioCodec::Flac);
}

QStringList ManualExportViewModel::flacCompressionOptions() const {
    return abracapture::exporting::AudioFormatCatalog::compressionLevelOptions(
        AudioContainer::Flac, AudioCodec::Flac);
}

QStringList ManualExportViewModel::wavSampleFormatOptions() const {
    return abracapture::exporting::AudioFormatCatalog::sampleFormatOptions(
        AudioContainer::Wav, AudioCodec::Pcm);
}

QStringList ManualExportViewModel::sampleRateOptions() const {
    const ExportProfile profile = activeProfile();
    return abracapture::exporting::AudioFormatCatalog::sampleRateOptions(
        profile.target.container, profile.target.codec);
}

QStringList ManualExportViewModel::sampleRateOptionLabels() const {
    const ExportProfile profile = activeProfile();
    return abracapture::exporting::AudioFormatCatalog::sampleRateOptionLabels(
        profile.target.container, profile.target.codec);
}

QStringList ManualExportViewModel::mp3BitrateOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::bitrateOptionLabels(
        AudioContainer::Mp3, AudioCodec::Mp3);
}

QStringList ManualExportViewModel::flacBitDepthOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::bitDepthOptionLabels(
        AudioContainer::Flac, AudioCodec::Flac);
}

QStringList ManualExportViewModel::flacCompressionOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::compressionLevelOptionLabels(
        AudioContainer::Flac, AudioCodec::Flac);
}

QStringList ManualExportViewModel::wavSampleFormatOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::sampleFormatOptionLabels(
        AudioContainer::Wav, AudioCodec::Pcm);
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
    emit bulkPreviewChanged();
}
QString ManualExportViewModel::customDestinationDirectory() const { return destination_.customDirectory; }
void ManualExportViewModel::setCustomDestinationDirectory(const QString& directory) {
    const QString next = QDir(directory).absolutePath();
    if (destination_.customDirectory == next) return;
    destination_.customDirectory = next;
    emit destinationChanged();
    emit bulkPreviewChanged();
}
bool ManualExportViewModel::useStoredMetadataNames() const noexcept { return destination_.useStoredMetadataNames; }
void ManualExportViewModel::setUseStoredMetadataNames(const bool enabled) {
    if (destination_.useStoredMetadataNames == enabled) return;
    destination_.useStoredMetadataNames = enabled;
    emit destinationChanged();
    emit bulkPreviewChanged();
}
bool ManualExportViewModel::organizeAlbumDirectory() const noexcept { return destination_.organizeAlbumDirectory; }
void ManualExportViewModel::setOrganizeAlbumDirectory(const bool enabled) {
    if (destination_.organizeAlbumDirectory == enabled) return;
    destination_.organizeAlbumDirectory = enabled;
    emit destinationChanged();
    emit bulkPreviewChanged();
}

QString ManualExportViewModel::collisionPolicy() const {
    if (!collisionPolicyExplicit_ && sources_.size() > 1) return QStringLiteral("skip");
    return collisionPolicyName(destination_.collisionPolicy);
}

void ManualExportViewModel::setCollisionPolicy(const QString& policy) {
    const auto next = collisionPolicyFromName(policy);
    if (collisionPolicyExplicit_ && destination_.collisionPolicy == next) return;
    destination_.collisionPolicy = next;
    collisionPolicyExplicit_ = true;
    emit destinationChanged();
    emit bulkPreviewChanged();
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

QVariantMap ManualExportViewModel::bulkPreview() const {
    int skipped = 0;
    QString error;
    const auto requests = plannedRequests(&skipped, &error);
    return {
        {QStringLiteral("sourceCount"), sources_.size()},
        {QStringLiteral("skippedCount"), skipped},
        {QStringLiteral("newCount"), requests.size()},
        {QStringLiteral("error"), error},
        {QStringLiteral("profile"), profileSummary()},
        {QStringLiteral("destination"), destination_.policy == ManualExportDestinationPolicy::ExportsSubfolder
            ? QStringLiteral("Exports/") : destination_.policy == ManualExportDestinationPolicy::CustomDirectory
                ? destination_.customDirectory : QStringLiteral("Same directory as source")}
    };
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
        selectedFile_.clear();
        selectedDirectory_.clear();
        selectedSessionId_.clear();
        setSources(QStringLiteral("recordings"), {});
        updateStatus(QStringLiteral("Select one or more masters on the Recordings page, then choose Export Selected"));
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
    int skipped = 0;
    QString planningError;
    QList<abracapture::exporting::ExportRequest> requests = plannedRequests(&skipped, &planningError);
    for (const auto& request : requests) {
        if (!QDir().mkpath(QString::fromStdString(request.destinationPath.parent_path().string()))) {
            if (planningError.isEmpty()) planningError = QStringLiteral("Unable to create export destination directory");
        }
    }
    if (!planningError.isEmpty() && requests.isEmpty()) updateStatus(planningError);
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
    updateStatus(QStringLiteral("Queued %1 export job%2; %3 skipped")
                     .arg(ids.size()).arg(ids.size() == 1 ? QString() : QStringLiteral("s"))
                     .arg(skipped));
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
    auto destination = destination_;
    destination.collisionPolicy = collisionPolicyFromName(collisionPolicy());
    return destination;
}

QList<abracapture::exporting::ExportRequest> ManualExportViewModel::plannedRequests(
    int* const skipped, QString* const error) const {
    if (skipped != nullptr) *skipped = 0;
    if (error != nullptr) error->clear();
    QList<abracapture::exporting::ExportRequest> requests;
    QSet<QString> reserved;
    const auto destination = activeDestination();
    const auto profile = activeProfile();
    for (const auto& source : sources_) {
        abracapture::exporting::ExportRequest baseRequest;
        QString planError;
        if (!abracapture::exporting::ManualExportPlanner::planRequest(
                source, profile, destination, false, &baseRequest, &planError)) {
            if (error != nullptr && error->isEmpty()) *error = planError;
            continue;
        }
        if (destination.collisionPolicy == abracapture::exporting::CollisionPolicy::FailIfExists &&
            (QFileInfo::exists(QString::fromStdString(baseRequest.destinationPath.string())) ||
             reserved.contains(abracapture::exporting::ExportOutputPlanner::normalizedPathKey(baseRequest.destinationPath)))) {
            if (skipped != nullptr) ++*skipped;
            continue;
        }
        abracapture::exporting::ExportRequest planned;
        if (!abracapture::exporting::ExportOutputPlanner::plan(
                baseRequest, reserved, &planned, &planError)) {
            if (error != nullptr && error->isEmpty()) *error = planError;
            continue;
        }
        reserved.insert(abracapture::exporting::ExportOutputPlanner::normalizedPathKey(planned.destinationPath));
        requests.append(std::move(planned));
    }
    return requests;
}

abracapture::exporting::ManualExportSource ManualExportViewModel::sourceFromVariant(
    const QVariantMap& entry) const {
    abracapture::exporting::ManualExportSource source;
    source.path = entry.value(QStringLiteral("filePath")).toString().toStdString();
    source.sourceFormat.sampleRate = entry.value(QStringLiteral("sampleRate")).toUInt();
    const QStringList posNames = entry.value(QStringLiteral("channelPositions")).toStringList();
    const auto chCount = static_cast<std::uint16_t>(entry.value(QStringLiteral("channels")).toUInt());
    if (!posNames.isEmpty()) {
        source.sourceFormat.channelLayout =
            abracapture::exporting::AudioChannelLayout::fromPositionNames(posNames, chCount);
    } else {
        source.sourceFormat.channelLayout =
            abracapture::exporting::AudioChannelLayout::unknown(chCount);
    }
    source.captureSessionId = entry.value(QStringLiteral("captureSessionId")).toString();
    source.trackTitle = entry.value(QStringLiteral("trackTitle")).toString();
    source.albumTitle = entry.value(QStringLiteral("albumTitle")).toString();
    return source;
}

QVariantMap ManualExportViewModel::sourceDetails(
    const abracapture::exporting::ManualExportSource& source) const {
    const QFileInfo info(QString::fromStdString(source.path.string()));
    int sampleRate = static_cast<int>(source.sourceFormat.sampleRate);
    int channels = 0;
    int bitDepth = 0;
    QString sampleFormat;
    QString channelLayoutName;
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
        if (audioInfo.channels > 0 && audioInfo.channels <= 64) {
            std::vector<int> channelMap(static_cast<std::size_t>(audioInfo.channels), 0);
            if (sf_command(audio, SFC_GET_CHANNEL_MAP_INFO, channelMap.data(),
                           static_cast<int>(sizeof(int) * channelMap.size())) == SF_TRUE) {
                QStringList positions;
                for (int i = 0; i < audioInfo.channels; ++i) {
                    switch (channelMap[static_cast<std::size_t>(i)]) {
                    case SF_CHANNEL_MAP_LEFT:
                    case SF_CHANNEL_MAP_FRONT_LEFT:
                        positions.append(QStringLiteral("FL")); break;
                    case SF_CHANNEL_MAP_RIGHT:
                    case SF_CHANNEL_MAP_FRONT_RIGHT:
                        positions.append(QStringLiteral("FR")); break;
                    case SF_CHANNEL_MAP_CENTER:
                    case SF_CHANNEL_MAP_FRONT_CENTER:
                    case SF_CHANNEL_MAP_MONO:
                        positions.append(QStringLiteral("FC")); break;
                    case SF_CHANNEL_MAP_LFE: positions.append(QStringLiteral("LFE")); break;
                    case SF_CHANNEL_MAP_REAR_LEFT: positions.append(QStringLiteral("RL")); break;
                    case SF_CHANNEL_MAP_REAR_RIGHT: positions.append(QStringLiteral("RR")); break;
                    case SF_CHANNEL_MAP_SIDE_LEFT: positions.append(QStringLiteral("SL")); break;
                    case SF_CHANNEL_MAP_SIDE_RIGHT: positions.append(QStringLiteral("SR")); break;
                    default: break;
                    }
                }
                if (positions.size() == audioInfo.channels) {
                    channelLayoutName = abracapture::exporting::AudioChannelLayout::fromPositionNames(positions).displayName();
                }
            }
        }
        sf_close(audio);
    }
    if (channelLayoutName.isEmpty()) {
        if (source.sourceFormat.channelLayout.channelCount() > 0) {
            channelLayoutName = source.sourceFormat.channelLayout.displayName();
        } else if (channels == 2) {
            channelLayoutName = QStringLiteral("Stereo");
        } else if (channels == 1) {
            channelLayoutName = QStringLiteral("Mono");
        } else if (channels > 0) {
            channelLayoutName = QStringLiteral("%1 channels — layout unknown").arg(channels);
        }
    }
    return {
        {QStringLiteral("filePath"), info.absoluteFilePath()},
        {QStringLiteral("fileName"), info.fileName()},
        {QStringLiteral("codec"), codec},
        {QStringLiteral("sampleRate"), sampleRate},
        {QStringLiteral("channels"), channels},
        {QStringLiteral("channelLayout"), channelLayoutName},
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
    emit bulkPreviewChanged();
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
            abracapture::history::DerivedExport derived =
                abracapture::exporting::createDerivedExportRecord(
                    jobId, request, state, outputPath, error);
            entry.derivedExports.append(derived);
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
