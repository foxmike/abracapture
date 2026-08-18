#pragma once

#include "ExportTypes.hpp"

#include <QHash>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <filesystem>
#include <optional>

namespace abracapture::exporting {

class ExportManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY queueChanged)
    Q_PROPERTY(int queuedJobCount READ queuedJobCount NOTIFY queueChanged)

public:
    explicit ExportManager(QObject* parent = nullptr);

    [[nodiscard]] ExportJobId enqueue(const ExportRequest& request);
    [[nodiscard]] QList<ExportJobId> enqueueAll(const QList<ExportRequest>& requests);
    [[nodiscard]] bool cancelQueued(ExportJobId jobId);

    [[nodiscard]] std::optional<ExportJob> job(ExportJobId jobId) const;
    [[nodiscard]] QList<ExportJob> jobs() const;
    [[nodiscard]] bool isBusy() const noexcept;
    [[nodiscard]] int queuedJobCount() const noexcept;
    Q_INVOKABLE QVariantMap currentJobSummary() const;
    Q_INVOKABLE QVariantList recentJobSummaries(int maximum = 5) const;

signals:
    void jobQueued(ExportJobId jobId);
    void jobStarted(ExportJobId jobId);
    void jobCompleted(ExportJobId jobId, QString outputPath);
    void jobFailed(ExportJobId jobId, QString error);
    void jobCancelled(ExportJobId jobId);
    void queueChanged();

    void exportStarted(QString format);
    void exportCompleted(QString path);
    void exportFailed(QString format, QString error);
    void allExportsFinished();

private:
    [[nodiscard]] ExportJobId enqueueInternal(const ExportRequest& request);
    void scheduleProcessing();
    void startNextExport();
    void captureProcessStandardError();
    [[nodiscard]] QString processFailureMessage(int exitCode) const;
    [[nodiscard]] std::filesystem::path temporaryOutputPathFor(const ExportJob& job) const;
    [[nodiscard]] bool sourceIsUsable(const ExportRequest& request, QString* error) const;
    void completeJob(ExportJobId jobId);
    void failJob(ExportJobId jobId, const QString& error);
    void releaseDestination(const ExportJob& job);

    QProcess process_;
    QQueue<ExportJobId> queuedJobIds_;
    QHash<ExportJobId, ExportJob> jobsById_;
    QList<ExportJobId> jobOrder_;
    QSet<QString> reservedOutputPaths_;
    std::optional<ExportJobId> runningJobId_;
    ExportJobId nextJobId_ = 1;
    QByteArray standardError_;
    bool processingScheduled_ = false;
    bool idle_ = true;
};

} // namespace abracapture::exporting
