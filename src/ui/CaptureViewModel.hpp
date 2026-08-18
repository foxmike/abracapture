#pragma once

#include "audio/CaptureSession.hpp"
#include "automation/AutoRecordController.hpp"

#include <QObject>
#include <QString>
#include <QtGlobal>

class QTimer;
class SettingsViewModel;
class SourceListModel;

class CaptureViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString captureState READ captureState NOTIFY captureStateChanged)
    Q_PROPERTY(quint32 selectedSourceId READ selectedSourceId WRITE setSelectedSourceId
                   NOTIFY selectedSourceIdChanged)
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString formatDescription READ formatDescription NOTIFY formatDescriptionChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY durationSecondsChanged)
    Q_PROPERTY(QString integrityStatus READ integrityStatus NOTIFY integrityStatusChanged)
    Q_PROPERTY(CaptureKind captureKind READ captureKind NOTIFY captureKindChanged)
    Q_PROPERTY(bool isNativeCapture READ isNativeCapture NOTIFY captureKindChanged)
    Q_PROPERTY(bool isQuantizedCapture READ isQuantizedCapture NOTIFY captureKindChanged)
    Q_PROPERTY(bool isResampledCapture READ isResampledCapture NOTIFY captureKindChanged)
    Q_PROPERTY(bool isConvertedCapture READ isConvertedCapture NOTIFY captureKindChanged)
    Q_PROPERTY(int preRollSeconds READ preRollSeconds NOTIFY preRollSecondsChanged)
    Q_PROPERTY(int postRollSeconds READ postRollSeconds NOTIFY postRollSecondsChanged)
    Q_PROPERTY(bool autoStartEnabled READ autoStartEnabled NOTIFY autoStartEnabledChanged)
    Q_PROPERTY(bool autoStopEnabled READ autoStopEnabled NOTIFY autoStopEnabledChanged)
    Q_PROPERTY(bool armed READ armed NOTIFY armedChanged)

public:
    enum CaptureKind {
        Native,
        Quantized,
        Resampled,
        Converted,
    };
    Q_ENUM(CaptureKind)

    explicit CaptureViewModel(SourceListModel* sourceListModel,
                              SettingsViewModel* settingsViewModel,
                              QObject* parent = nullptr);

    [[nodiscard]] QString captureState() const;
    [[nodiscard]] quint32 selectedSourceId() const noexcept;
    void setSelectedSourceId(quint32 sourceId);
    [[nodiscard]] QString outputPath() const;
    void setOutputPath(const QString& outputPath);
    [[nodiscard]] QString formatDescription() const;
    [[nodiscard]] double durationSeconds() const noexcept;
    [[nodiscard]] QString integrityStatus() const;
    [[nodiscard]] CaptureKind captureKind() const noexcept;
    [[nodiscard]] bool isNativeCapture() const noexcept;
    [[nodiscard]] bool isQuantizedCapture() const noexcept;
    [[nodiscard]] bool isResampledCapture() const noexcept;
    [[nodiscard]] bool isConvertedCapture() const noexcept;
    [[nodiscard]] int preRollSeconds() const noexcept;
    [[nodiscard]] int postRollSeconds() const noexcept;
    [[nodiscard]] bool autoStartEnabled() const noexcept;
    [[nodiscard]] bool autoStopEnabled() const noexcept;
    [[nodiscard]] bool armed() const noexcept;

    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void stopCapture();
    Q_INVOKABLE void armCapture();
    Q_INVOKABLE void disarmCapture();
    Q_INVOKABLE void feedAudioLevel(float peakLinear, bool newAudioGeneration = true);
    Q_INVOKABLE void reportCaptureError(const QString& message);
    [[nodiscard]] bool completeAutomaticRecording();

public slots:
    void handleSessionState(abracapture::audio::SessionState state);
    void updateEngineFormat(const QString& description, CaptureKind kind);
    void updateIntegrityMetrics(qulonglong overflowCount);

signals:
    void captureStateChanged();
    void selectedSourceIdChanged();
    void outputPathChanged();
    void formatDescriptionChanged();
    void durationSecondsChanged();
    void integrityStatusChanged();
    void captureKindChanged();
    void preRollSecondsChanged();
    void postRollSecondsChanged();
    void autoStartEnabledChanged();
    void autoStopEnabledChanged();
    void armedChanged();

    // These requests are consumed by the application/engine composition layer.
    // The view model deliberately has no recording, compression, or PipeWire work.
    void startCaptureRequested(quint32 sourceId);
    void stopCaptureRequested();

private slots:
    void updateDuration();

private:
    void setCaptureState(const QString& state);
    void setIntegrityStatus(const QString& status);
    void resetDuration();
    void stopAutomaticRecording();

    SourceListModel* sourceListModel_ = nullptr;
    SettingsViewModel* settingsViewModel_ = nullptr;
    QTimer* durationTimer_ = nullptr;
    AutoRecordController autoRecordController_;

    QString captureState_ = QStringLiteral("idle");
    quint32 selectedSourceId_ = 0;
    QString outputPath_;
    QString formatDescription_ = QStringLiteral("Waiting for source format");
    double durationSeconds_ = 0.0;
    QString integrityStatus_ = QStringLiteral("Ready");
    CaptureKind captureKind_ = Native;
    bool armed_ = false;
    bool awaitFreshSignal_ = false;
    bool startThresholdWaitingLogged_ = false;
    bool automaticStopPending_ = false;
    qint64 captureStartedAtMs_ = 0;
};
