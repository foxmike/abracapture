#pragma once

#include "export/ExportManager.hpp"
#include "export/ManualExportPlanner.hpp"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace abracapture::history { class RecordingIndex; }
class SettingsViewModel;

class ManualExportViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString sourceMode READ sourceMode NOTIFY sourceChanged)
    Q_PROPERTY(QString selectedFile READ selectedFile NOTIFY sourceChanged)
    Q_PROPERTY(QString selectedDirectory READ selectedDirectory NOTIFY sourceChanged)
    Q_PROPERTY(bool includeSubdirectories READ includeSubdirectories WRITE setIncludeSubdirectories NOTIFY sourceChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourceChanged)
    Q_PROPERTY(int sourceCount READ sourceCount NOTIFY sourceChanged)
    Q_PROPERTY(QVariantList captureSessions READ captureSessions NOTIFY captureSessionsChanged)
    Q_PROPERTY(QString selectedSessionId READ selectedSessionId NOTIFY sourceChanged)
    Q_PROPERTY(bool useAutomaticProfile READ useAutomaticProfile WRITE setUseAutomaticProfile NOTIFY profileChanged)
    Q_PROPERTY(QString manualFormat READ manualFormat WRITE setManualFormat NOTIFY profileChanged)
    Q_PROPERTY(int manualSampleRate READ manualSampleRate WRITE setManualSampleRate NOTIFY profileChanged)
    Q_PROPERTY(int manualMp3Bitrate READ manualMp3Bitrate WRITE setManualMp3Bitrate NOTIFY profileChanged)
    Q_PROPERTY(int manualFlacBitDepth READ manualFlacBitDepth WRITE setManualFlacBitDepth NOTIFY profileChanged)
    Q_PROPERTY(int manualFlacCompressionLevel READ manualFlacCompressionLevel WRITE setManualFlacCompressionLevel NOTIFY profileChanged)
    Q_PROPERTY(QString manualWavSampleFormat READ manualWavSampleFormat WRITE setManualWavSampleFormat NOTIFY profileChanged)
    Q_PROPERTY(QString manualPreset READ manualPreset WRITE setManualPreset NOTIFY profileChanged)
    Q_PROPERTY(QStringList presetOptions READ presetOptions CONSTANT)
    Q_PROPERTY(QStringList formatOptions READ formatOptions CONSTANT)
    Q_PROPERTY(QStringList mp3BitrateOptions READ mp3BitrateOptions CONSTANT)
    Q_PROPERTY(QStringList flacBitDepthOptions READ flacBitDepthOptions CONSTANT)
    Q_PROPERTY(QStringList flacCompressionOptions READ flacCompressionOptions CONSTANT)
    Q_PROPERTY(QStringList wavSampleFormatOptions READ wavSampleFormatOptions CONSTANT)
    Q_PROPERTY(QStringList sampleRateOptions READ sampleRateOptions NOTIFY profileChanged)
    Q_PROPERTY(QString profileSummary READ profileSummary NOTIFY profileChanged)
    Q_PROPERTY(QString destinationPolicy READ destinationPolicy WRITE setDestinationPolicy NOTIFY destinationChanged)
    Q_PROPERTY(QString customDestinationDirectory READ customDestinationDirectory WRITE setCustomDestinationDirectory NOTIFY destinationChanged)
    Q_PROPERTY(bool useStoredMetadataNames READ useStoredMetadataNames WRITE setUseStoredMetadataNames NOTIFY destinationChanged)
    Q_PROPERTY(bool organizeAlbumDirectory READ organizeAlbumDirectory WRITE setOrganizeAlbumDirectory NOTIFY destinationChanged)
    Q_PROPERTY(QString destinationPreview READ destinationPreview NOTIFY destinationChanged)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY jobsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit ManualExportViewModel(abracapture::exporting::ExportManager* exportManager,
                                   abracapture::history::RecordingIndex* recordingIndex,
                                   SettingsViewModel* settingsViewModel,
                                   QObject* parent = nullptr);

    [[nodiscard]] QString sourceMode() const;
    [[nodiscard]] QString selectedFile() const;
    [[nodiscard]] QString selectedDirectory() const;
    [[nodiscard]] bool includeSubdirectories() const noexcept;
    void setIncludeSubdirectories(bool enabled);
    [[nodiscard]] QVariantList sources() const;
    [[nodiscard]] int sourceCount() const noexcept;
    [[nodiscard]] QVariantList captureSessions() const;
    [[nodiscard]] QString selectedSessionId() const;

    [[nodiscard]] bool useAutomaticProfile() const noexcept;
    void setUseAutomaticProfile(bool enabled);
    [[nodiscard]] QString manualFormat() const;
    void setManualFormat(const QString& format);
    [[nodiscard]] int manualSampleRate() const noexcept;
    void setManualSampleRate(int sampleRate);
    [[nodiscard]] int manualMp3Bitrate() const noexcept;
    void setManualMp3Bitrate(int bitrateKbps);
    [[nodiscard]] int manualFlacBitDepth() const noexcept;
    void setManualFlacBitDepth(int bitDepth);
    [[nodiscard]] int manualFlacCompressionLevel() const noexcept;
    void setManualFlacCompressionLevel(int level);
    [[nodiscard]] QString manualWavSampleFormat() const;
    void setManualWavSampleFormat(const QString& sampleFormat);
    [[nodiscard]] QString manualPreset() const;
    void setManualPreset(const QString& preset);
    [[nodiscard]] QStringList presetOptions() const;
    [[nodiscard]] QStringList formatOptions() const;
    [[nodiscard]] QStringList mp3BitrateOptions() const;
    [[nodiscard]] QStringList flacBitDepthOptions() const;
    [[nodiscard]] QStringList flacCompressionOptions() const;
    [[nodiscard]] QStringList wavSampleFormatOptions() const;
    [[nodiscard]] QStringList sampleRateOptions() const;
    [[nodiscard]] QString profileSummary() const;

    [[nodiscard]] QString destinationPolicy() const;
    void setDestinationPolicy(const QString& policy);
    [[nodiscard]] QString customDestinationDirectory() const;
    void setCustomDestinationDirectory(const QString& directory);
    [[nodiscard]] bool useStoredMetadataNames() const noexcept;
    void setUseStoredMetadataNames(bool enabled);
    [[nodiscard]] bool organizeAlbumDirectory() const noexcept;
    void setOrganizeAlbumDirectory(bool enabled);
    [[nodiscard]] QString destinationPreview() const;
    [[nodiscard]] QVariantList jobs() const;
    [[nodiscard]] QString statusMessage() const;

    Q_INVOKABLE bool selectFile(const QString& pathOrUrl);
    Q_INVOKABLE bool selectDirectory(const QString& directory);
    Q_INVOKABLE void refreshDirectory();
    Q_INVOKABLE void setSelectedRecordings(const QVariantList& entries);
    Q_INVOKABLE bool activateSelectedRecordings();
    Q_INVOKABLE bool selectMostRecentCaptureSession();
    Q_INVOKABLE bool selectCaptureSession(const QString& sessionId);
    Q_INVOKABLE void refreshCaptureSessions();
    Q_INVOKABLE int enqueueExport();
    Q_INVOKABLE bool retry(qulonglong jobId);
    Q_INVOKABLE bool cancelQueued(qulonglong jobId);

signals:
    void sourceChanged();
    void captureSessionsChanged();
    void profileChanged();
    void destinationChanged();
    void jobsChanged();
    void statusMessageChanged();
    void recordingHistoryChanged();

private:
    [[nodiscard]] abracapture::exporting::ExportProfile activeProfile() const;
    [[nodiscard]] abracapture::exporting::ManualExportDestination activeDestination() const;
    [[nodiscard]] abracapture::exporting::ManualExportSource sourceFromVariant(
        const QVariantMap& entry) const;
    [[nodiscard]] QVariantMap sourceDetails(const abracapture::exporting::ManualExportSource& source) const;
    void setSources(QString mode, QList<abracapture::exporting::ManualExportSource> sources);
    void rebuildCaptureSessions();
    void updateDerivedExport(abracapture::exporting::ExportJobId jobId, const QString& state,
                             const QString& outputPath, const QString& error);
    void updateStatus(QString message);

    abracapture::exporting::ExportManager* exportManager_ = nullptr;
    abracapture::history::RecordingIndex* recordingIndex_ = nullptr;
    SettingsViewModel* settingsViewModel_ = nullptr;
    QString sourceMode_;
    QString selectedFile_;
    QString selectedDirectory_;
    bool includeSubdirectories_ = false;
    QList<abracapture::exporting::ManualExportSource> sources_;
    QList<abracapture::exporting::ManualExportSource> selectedRecordingSources_;
    QHash<QString, QList<abracapture::exporting::ManualExportSource>> sessionSources_;
    QVariantList captureSessions_;
    QString selectedSessionId_;
    bool useAutomaticProfile_ = true;
    abracapture::exporting::ExportProfile manualProfile_;
    abracapture::exporting::ManualExportDestination destination_;
    QHash<abracapture::exporting::ExportJobId, abracapture::exporting::ExportRequest> manualRequests_;
    QList<abracapture::exporting::ExportJobId> manualJobOrder_;
    QHash<abracapture::exporting::ExportJobId, QString> recordingPathsByJob_;
    QString statusMessage_;
};
