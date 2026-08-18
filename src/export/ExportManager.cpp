#include "ExportManager.hpp"

#include "ExportOutputPlanner.hpp"
#include "FfmpegArgumentBuilder.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

#include <system_error>

namespace abracapture::exporting {

namespace {

constexpr qsizetype kMaximumDiagnosticBytes = 16 * 1024;

QString userFacingProcessError(const QString& diagnostics, const int exitCode) {
    const QString detail = diagnostics.trimmed();
    const QString lower = detail.toLower();
    if (lower.contains(QStringLiteral("permission denied")) ||
        lower.contains(QStringLiteral("access is denied"))) {
        return QStringLiteral("Permission denied while writing the export");
    }
    if (lower.contains(QStringLiteral("unknown encoder")) ||
        lower.contains(QStringLiteral("invalid argument")) ||
        lower.contains(QStringLiteral("does not support"))) {
        return QStringLiteral("Unsupported format or encoder combination");
    }
    if (lower.contains(QStringLiteral("no such file")) ||
        lower.contains(QStringLiteral("input file"))) {
        return QStringLiteral("Source file is missing or cannot be read");
    }
    if (detail.isEmpty()) {
        return QStringLiteral("ffmpeg conversion failed (exit code %1)").arg(exitCode);
    }
    // Keep normal UI/status text actionable and compact. The bounded stderr is
    // still captured for diagnostics and can be added to a details view later.
    const QString firstLine = detail.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)
                                  .constFirst().trimmed();
    return QStringLiteral("ffmpeg conversion failed: %1").arg(firstLine.left(240));
}

} // namespace

ExportManager::ExportManager(QObject* const parent)
    : QObject(parent),
      process_(this) {
    connect(&process_, &QProcess::readyReadStandardError,
            this, &ExportManager::captureProcessStandardError);
    connect(&process_, &QProcess::finished, this,
            [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
                if (!runningJobId_.has_value()) {
                    return;
                }

                captureProcessStandardError();
                const ExportJobId jobId = *runningJobId_;
                runningJobId_.reset();
                const auto jobIt = jobsById_.constFind(jobId);
                if (jobIt == jobsById_.cend()) {
                    scheduleProcessing();
                    return;
                }

                const ExportJob& job = *jobIt;
                const QFileInfo temporaryInfo(
                    QString::fromStdString(job.temporaryOutputPath.string()));
                if (exitStatus == QProcess::NormalExit && exitCode == 0 &&
                    temporaryInfo.exists() && temporaryInfo.isFile() && temporaryInfo.size() > 0) {
                    const QString temporaryPath = temporaryInfo.absoluteFilePath();
                    const QString finalPath = QString::fromStdString(job.request.destinationPath.string());
                    if (QFileInfo::exists(finalPath) &&
                        job.request.profile.collisionPolicy != CollisionPolicy::ReplaceExisting) {
                        failJob(jobId, QStringLiteral("Export destination appeared before output could be committed"));
                    } else {
                        if (QFileInfo::exists(finalPath) && !QFile::remove(finalPath)) {
                            failJob(jobId, QStringLiteral("Unable to replace existing export output"));
                            scheduleProcessing();
                            return;
                        }
                        QFile temporaryFile(temporaryPath);
                        if (!temporaryFile.rename(finalPath)) {
                            failJob(jobId, QStringLiteral("Unable to commit export output: %1")
                                               .arg(temporaryFile.errorString()));
                        } else {
                            completeJob(jobId);
                        }
                    }
                } else if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    failJob(jobId,
                            QStringLiteral("ffmpeg completed but the temporary export output is missing or empty"));
                } else {
                    failJob(jobId, processFailureMessage(exitCode));
                }
                scheduleProcessing();
            });

    connect(&process_, &QProcess::errorOccurred, this,
            [this](const QProcess::ProcessError error) {
                if (!runningJobId_.has_value() || error != QProcess::FailedToStart) {
                    return;
                }
                captureProcessStandardError();
                const ExportJobId jobId = *runningJobId_;
                runningJobId_.reset();
                failJob(jobId, QStringLiteral("ffmpeg is unavailable or could not be started: %1")
                                   .arg(process_.errorString()));
                scheduleProcessing();
            });
}

ExportJobId ExportManager::enqueue(const ExportRequest& request) {
    const QList<ExportJobId> jobIds = enqueueAll({request});
    return jobIds.constFirst();
}

QList<ExportJobId> ExportManager::enqueueAll(const QList<ExportRequest>& requests) {
    QList<ExportJobId> jobIds;
    jobIds.reserve(requests.size());
    for (const ExportRequest& request : requests) {
        jobIds.append(enqueueInternal(request));
    }
    if (!jobIds.isEmpty()) {
        scheduleProcessing();
    }
    return jobIds;
}

bool ExportManager::cancelQueued(const ExportJobId jobId) {
    auto jobIt = jobsById_.find(jobId);
    if (jobIt == jobsById_.end() || jobIt->state != ExportJobState::Queued) {
        return false;
    }
    QQueue<ExportJobId> retained;
    while (!queuedJobIds_.isEmpty()) {
        const ExportJobId queuedId = queuedJobIds_.dequeue();
        if (queuedId != jobId) {
            retained.enqueue(queuedId);
        }
    }
    queuedJobIds_ = std::move(retained);
    jobIt->state = ExportJobState::Cancelled;
    jobIt->finishedAt = QDateTime::currentDateTimeUtc();
    jobIt->error.clear();
    releaseDestination(*jobIt);
    emit jobCancelled(jobId);
    emit queueChanged();
    scheduleProcessing();
    return true;
}

std::optional<ExportJob> ExportManager::job(const ExportJobId jobId) const {
    const auto it = jobsById_.constFind(jobId);
    if (it == jobsById_.cend()) {
        return std::nullopt;
    }
    return *it;
}

QList<ExportJob> ExportManager::jobs() const {
    QList<ExportJob> orderedJobs;
    orderedJobs.reserve(jobOrder_.size());
    for (const ExportJobId jobId : jobOrder_) {
        const auto it = jobsById_.constFind(jobId);
        if (it != jobsById_.cend()) {
            orderedJobs.append(*it);
        }
    }
    return orderedJobs;
}

bool ExportManager::isBusy() const noexcept {
    return runningJobId_.has_value() || !queuedJobIds_.isEmpty();
}

int ExportManager::queuedJobCount() const noexcept {
    return queuedJobIds_.size();
}

QVariantMap ExportManager::currentJobSummary() const {
    if (!runningJobId_.has_value()) {
        return {};
    }
    const auto current = job(*runningJobId_);
    if (!current.has_value()) {
        return {};
    }
    return {
        {QStringLiteral("jobId"), QVariant::fromValue<qulonglong>(current->id)},
        {QStringLiteral("state"), QStringLiteral("running")},
        {QStringLiteral("sourcePath"), QString::fromStdString(current->request.sourcePath.string())},
        {QStringLiteral("outputPath"), QString::fromStdString(current->request.destinationPath.string())},
    };
}

QVariantList ExportManager::recentJobSummaries(const int maximum) const {
    QVariantList result;
    if (maximum <= 0) {
        return result;
    }
    for (auto iterator = jobOrder_.crbegin(); iterator != jobOrder_.crend() && result.size() < maximum;
         ++iterator) {
        const auto current = job(*iterator);
        if (!current.has_value() || current->state == ExportJobState::Queued ||
            current->state == ExportJobState::Running) {
            continue;
        }
        QString state;
        switch (current->state) {
        case ExportJobState::Completed: state = QStringLiteral("completed"); break;
        case ExportJobState::Failed: state = QStringLiteral("failed"); break;
        case ExportJobState::Cancelled: state = QStringLiteral("cancelled"); break;
        case ExportJobState::Queued:
        case ExportJobState::Running: break;
        }
        result.append(QVariantMap{
            {QStringLiteral("jobId"), QVariant::fromValue<qulonglong>(current->id)},
            {QStringLiteral("state"), state},
            {QStringLiteral("sourcePath"), QString::fromStdString(current->request.sourcePath.string())},
            {QStringLiteral("outputPath"), QString::fromStdString(
                                                  (current->resultPath.empty()
                                                       ? current->request.destinationPath
                                                       : current->resultPath).string())},
            {QStringLiteral("error"), current->error},
        });
    }
    return result;
}

bool ExportManager::verifyMaster(const std::filesystem::path& masterFilePath,
                                 const recording::CaptureIntegrity& integrity) const {
    const QFileInfo masterInfo(QString::fromStdString(masterFilePath.string()));
    if (!masterInfo.exists() || !masterInfo.isFile() || masterInfo.size() == 0) {
        return false;
    }

    if (integrity.filePath.empty()) {
        return true;
    }

    const QFileInfo integrityInfo(QString::fromStdString(integrity.filePath.string()));
    return QDir::cleanPath(masterInfo.absoluteFilePath()) ==
           QDir::cleanPath(integrityInfo.absoluteFilePath());
}

ExportJobId ExportManager::enqueueInternal(const ExportRequest& request) {
    ExportJob job;
    job.id = nextJobId_++;
    job.request = request;
    job.state = ExportJobState::Queued;
    job.queuedAt = QDateTime::currentDateTimeUtc();
    const ExportJobId jobId = job.id;
    jobsById_.insert(jobId, job);
    jobOrder_.append(jobId);
    idle_ = false;
    emit jobQueued(jobId);
    emit queueChanged();

    QString error;
    if (!sourceIsUsable(request, &error)) {
        failJob(jobId, error);
        return jobId;
    }

    ExportRequest plannedRequest;
    if (!ExportOutputPlanner::plan(request, reservedOutputPaths_, &plannedRequest, &error)) {
        failJob(jobId, error);
        return jobId;
    }

    auto jobIt = jobsById_.find(jobId);
    jobIt->request = std::move(plannedRequest);
    reservedOutputPaths_.insert(ExportOutputPlanner::normalizedPathKey(jobIt->request.destinationPath));
    jobIt->destinationReserved = true;
    queuedJobIds_.enqueue(jobId);
    emit queueChanged();
    return jobId;
}

void ExportManager::scheduleProcessing() {
    if (processingScheduled_) {
        return;
    }
    processingScheduled_ = true;
    QTimer::singleShot(0, this, [this] {
        processingScheduled_ = false;
        startNextExport();
    });
}

void ExportManager::startNextExport() {
    if (runningJobId_.has_value()) {
        return;
    }

    while (!queuedJobIds_.isEmpty()) {
        const ExportJobId jobId = queuedJobIds_.dequeue();
        auto jobIt = jobsById_.find(jobId);
        if (jobIt == jobsById_.end() || jobIt->state != ExportJobState::Queued) {
            continue;
        }

        QString error;
        if (!sourceIsUsable(jobIt->request, &error)) {
            failJob(jobId, error);
            continue;
        }
        if (QFileInfo::exists(QString::fromStdString(jobIt->request.destinationPath.string())) &&
            jobIt->request.profile.collisionPolicy != CollisionPolicy::ReplaceExisting) {
            failJob(jobId, QStringLiteral("Export destination already exists"));
            continue;
        }

        jobIt->temporaryOutputPath = temporaryOutputPathFor(*jobIt);
        if (jobIt->temporaryOutputPath.empty()) {
            failJob(jobId, QStringLiteral("Unable to allocate a temporary export output path"));
            continue;
        }

        ExportRequest processRequest = jobIt->request;
        processRequest.destinationPath = jobIt->temporaryOutputPath;
        QStringList arguments;
        if (!FfmpegArgumentBuilder::build(processRequest, &arguments, &error)) {
            failJob(jobId, error);
            continue;
        }

        jobIt->state = ExportJobState::Running;
        jobIt->startedAt = QDateTime::currentDateTimeUtc();
        standardError_.clear();
        runningJobId_ = jobId;
        emit jobStarted(jobId);
        emit exportStarted(exportFormatName(jobIt->request.profile.format));
        emit queueChanged();
        process_.start(QStringLiteral("ffmpeg"), arguments);
        return;
    }

    if (!idle_) {
        idle_ = true;
        emit queueChanged();
        emit allExportsFinished();
    }
}

void ExportManager::captureProcessStandardError() {
    standardError_.append(process_.readAllStandardError());
    if (standardError_.size() > kMaximumDiagnosticBytes) {
        standardError_ = standardError_.right(kMaximumDiagnosticBytes);
    }
}

QString ExportManager::processFailureMessage(const int exitCode) const {
    return userFacingProcessError(QString::fromLocal8Bit(standardError_), exitCode);
}

std::filesystem::path ExportManager::temporaryOutputPathFor(const ExportJob& job) const {
    const std::filesystem::path finalPath = job.request.destinationPath;
    const std::filesystem::path parent = finalPath.parent_path();
    const std::string stem = finalPath.stem().string();
    const std::string extension = finalPath.extension().string();
    for (int suffix = 0; suffix <= 9999; ++suffix) {
        const std::string discriminator = suffix == 0
            ? ".abracapture-export-" + std::to_string(job.id)
            : ".abracapture-export-" + std::to_string(job.id) + "." + std::to_string(suffix);
        const std::filesystem::path candidate = parent / (stem + discriminator + ".tmp" + extension);
        std::error_code error;
        if (!std::filesystem::exists(candidate, error) && !error) {
            return candidate;
        }
    }
    return {};
}

bool ExportManager::sourceIsUsable(const ExportRequest& request, QString* const error) const {
    if (request.sourcePath.empty()) {
        *error = QStringLiteral("Export source path is empty");
        return false;
    }
    const QFileInfo sourceInfo(QString::fromStdString(request.sourcePath.string()));
    if (!sourceInfo.exists() || !sourceInfo.isFile() || sourceInfo.size() == 0) {
        *error = QStringLiteral("Export source is missing, not a regular file, or empty");
        return false;
    }
    return true;
}

void ExportManager::completeJob(const ExportJobId jobId) {
    auto jobIt = jobsById_.find(jobId);
    if (jobIt == jobsById_.end()) {
        return;
    }
    jobIt->state = ExportJobState::Completed;
    jobIt->finishedAt = QDateTime::currentDateTimeUtc();
    jobIt->resultPath = jobIt->request.destinationPath;
    jobIt->temporaryOutputPath.clear();
    jobIt->error.clear();
    releaseDestination(*jobIt);
    const QString outputPath = QString::fromStdString(jobIt->resultPath.string());
    emit jobCompleted(jobId, outputPath);
    emit exportCompleted(outputPath);
    emit queueChanged();
}

void ExportManager::failJob(const ExportJobId jobId, const QString& error) {
    auto jobIt = jobsById_.find(jobId);
    if (jobIt == jobsById_.end()) {
        return;
    }
    if (!jobIt->temporaryOutputPath.empty()) {
        std::error_code removeError;
        std::filesystem::remove(jobIt->temporaryOutputPath, removeError);
    }
    jobIt->state = ExportJobState::Failed;
    jobIt->finishedAt = QDateTime::currentDateTimeUtc();
    jobIt->error = error;
    releaseDestination(*jobIt);
    emit jobFailed(jobId, error);
    emit exportFailed(exportFormatName(jobIt->request.profile.format), error);
    emit queueChanged();
}

void ExportManager::releaseDestination(const ExportJob& job) {
    if (job.destinationReserved && !job.request.destinationPath.empty()) {
        reservedOutputPaths_.remove(ExportOutputPlanner::normalizedPathKey(job.request.destinationPath));
    }
}

} // namespace abracapture::exporting
