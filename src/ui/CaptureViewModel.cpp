#include "CaptureViewModel.hpp"

#include "NotificationService.hpp"
#include "SettingsViewModel.hpp"
#include "SourceListModel.hpp"

#include <QDateTime>
#include <QMetaObject>
#include <QTimer>
#include <QUuid>

#include <cmath>
#include <cstdio>
#include <limits>

CaptureViewModel::CaptureViewModel(SourceListModel* const sourceListModel,
                                   SettingsViewModel* const settingsViewModel,
                                   QObject* const parent)
    : QObject(parent),
      sourceListModel_(sourceListModel),
      settingsViewModel_(settingsViewModel),
      durationTimer_(new QTimer(this)) {
    durationTimer_->setInterval(250);
    connect(durationTimer_, &QTimer::timeout, this, &CaptureViewModel::updateDuration);

    connect(&autoRecordController_, &AutoRecordController::startRequested,
            this, &CaptureViewModel::startCapture);
    connect(&autoRecordController_, &AutoRecordController::automaticStopRequested,
            this, &CaptureViewModel::stopAutomaticRecording);

    auto* const notificationService = new NotificationService(this);

    connect(this, &CaptureViewModel::startCaptureRequested,
            notificationService, [this, notificationService](const quint32 sourceId) {
                QString sourceName;
                if (sourceListModel_ != nullptr) {
                    const QVariantMap source = sourceListModel_->getSourceById(sourceId);
                    sourceName = source.value(QStringLiteral("displayName")).toString();
                    if (sourceName.isEmpty()) {
                        sourceName = source.value(QStringLiteral("appName")).toString();
                    }
                }
                notificationService->onCaptureStarted(sourceName);
            }, Qt::QueuedConnection);

    if (sourceListModel_ != nullptr) {
        selectedSourceId_ = sourceListModel_->selectedSourceId();
        connect(sourceListModel_, &SourceListModel::sourceSelected,
                this, &CaptureViewModel::setSelectedSourceId);
        connect(sourceListModel_, &SourceListModel::selectionChanged,
                this, [this] {
                    setSelectedSourceId(sourceListModel_->selectedSourceId());
                });
    }

    if (settingsViewModel_ != nullptr) {
        outputPath_ = settingsViewModel_->outputDirectory();
        connect(settingsViewModel_, &SettingsViewModel::outputDirectoryChanged,
                this, [this](const QString& path) {
                    if (outputPath_ == path) {
                        return;
                    }
                    outputPath_ = path;
                    emit outputPathChanged();
                });
        connect(settingsViewModel_, &SettingsViewModel::preRollSecondsChanged,
                this, &CaptureViewModel::preRollSecondsChanged);
        connect(settingsViewModel_, &SettingsViewModel::postRollSecondsChanged,
                this, &CaptureViewModel::postRollSecondsChanged);
        connect(settingsViewModel_, &SettingsViewModel::autoStartEnabledChanged,
                this, &CaptureViewModel::autoStartEnabledChanged);
        connect(settingsViewModel_, &SettingsViewModel::autoStopEnabledChanged,
                this, &CaptureViewModel::autoStopEnabledChanged);
        connect(settingsViewModel_, &SettingsViewModel::masterFormatChanged,
                this, [this](const QString&) { emit formatDescriptionChanged(); });
        connect(settingsViewModel_, &SettingsViewModel::autoStartThresholdChanged,
                this, [this](const double threshold) {
                    autoRecordController_.setStartThreshold(static_cast<float>(threshold));
                });
        connect(settingsViewModel_, &SettingsViewModel::preRollSecondsChanged,
                this, [this](const int seconds) {
                    autoRecordController_.setPreRollDurationMs(seconds * 1000);
                });
        connect(settingsViewModel_, &SettingsViewModel::postRollSecondsChanged,
                this, [this](const int seconds) {
                    autoRecordController_.setPostRollDurationMs(seconds * 1000);
                });
        connect(settingsViewModel_, &SettingsViewModel::autoStopSilenceDurationChanged,
                this, [this](const int seconds) {
                    autoRecordController_.setSilenceDurationMs(seconds * 1000);
                });
        connect(settingsViewModel_, &SettingsViewModel::autoStartEnabledChanged,
                this, [this](const bool enabled) {
                    autoRecordController_.setAutoStartEnabled(enabled);
                });
        connect(settingsViewModel_, &SettingsViewModel::autoStopEnabledChanged,
                this, [this](const bool enabled) {
                    autoRecordController_.setAutoStopEnabled(enabled);
                });
        autoRecordController_.setStartThreshold(
            static_cast<float>(settingsViewModel_->autoStartThreshold()));
        autoRecordController_.setPreRollDurationMs(
            settingsViewModel_->preRollSeconds() * 1000);
        autoRecordController_.setPostRollDurationMs(
            settingsViewModel_->postRollSeconds() * 1000);
        autoRecordController_.setSilenceDurationMs(
            settingsViewModel_->autoStopSilenceDuration() * 1000);
        autoRecordController_.setAutoStartEnabled(settingsViewModel_->autoStartEnabled());
        autoRecordController_.setAutoStopEnabled(settingsViewModel_->autoStopEnabled());
    }
}

QString CaptureViewModel::captureState() const {
    return captureState_;
}

quint32 CaptureViewModel::selectedSourceId() const noexcept {
    return selectedSourceId_;
}

void CaptureViewModel::setSelectedSourceId(const quint32 sourceId) {
    if (selectedSourceId_ == sourceId) {
        return;
    }
    selectedSourceId_ = sourceId;
    emit selectedSourceIdChanged();
}

QString CaptureViewModel::outputPath() const {
    return outputPath_;
}

void CaptureViewModel::setOutputPath(const QString& outputPath) {
    const QString normalizedPath = outputPath.trimmed();
    if (outputPath_ == normalizedPath) {
        return;
    }
    outputPath_ = normalizedPath;
    emit outputPathChanged();
}

QString CaptureViewModel::formatDescription() const {
    return formatDescription_;
}

double CaptureViewModel::durationSeconds() const noexcept {
    return durationSeconds_;
}

QString CaptureViewModel::integrityStatus() const {
    return integrityStatus_;
}

CaptureViewModel::CaptureKind CaptureViewModel::captureKind() const noexcept {
    return captureKind_;
}

bool CaptureViewModel::isNativeCapture() const noexcept {
    return captureKind_ == Native;
}

bool CaptureViewModel::isQuantizedCapture() const noexcept {
    return captureKind_ == Quantized;
}

bool CaptureViewModel::isResampledCapture() const noexcept {
    return captureKind_ == Resampled;
}

bool CaptureViewModel::isConvertedCapture() const noexcept {
    return captureKind_ == Converted;
}

int CaptureViewModel::preRollSeconds() const noexcept {
    return settingsViewModel_ != nullptr ? settingsViewModel_->preRollSeconds() : 0;
}

int CaptureViewModel::postRollSeconds() const noexcept {
    return settingsViewModel_ != nullptr ? settingsViewModel_->postRollSeconds() : 0;
}

bool CaptureViewModel::autoStartEnabled() const noexcept {
    return settingsViewModel_ != nullptr && settingsViewModel_->autoStartEnabled();
}

bool CaptureViewModel::autoStopEnabled() const noexcept {
    return settingsViewModel_ != nullptr && settingsViewModel_->autoStopEnabled();
}

bool CaptureViewModel::armed() const noexcept {
    return armed_;
}

QString CaptureViewModel::captureSessionId() const {
    return captureSessionId_;
}

void CaptureViewModel::startCapture() {
    if (selectedSourceId_ == 0) {
        setIntegrityStatus(QStringLiteral("Select an audio source before starting."));
        return;
    }
    if (captureState_ == QStringLiteral("capturing") ||
        captureState_ == QStringLiteral("connecting")) {
        return;
    }

    const bool automaticStart = autoRecordController_.state() == AutoRecordController::Recording;
    if (!armed_ || captureSessionId_.isEmpty()) {
        beginCaptureSession();
    }
    if (!automaticStart) {
        armed_ = false;
        awaitFreshSignal_ = false;
        startThresholdWaitingLogged_ = false;
        emit armedChanged();
        autoRecordController_.reset();
    }
    resetDuration();
    setIntegrityStatus(QStringLiteral("Connecting to source…"));
    setCaptureState(QStringLiteral("connecting"));
    emit startCaptureRequested(selectedSourceId_);
}

void CaptureViewModel::stopCapture() {
    if (captureState_ != QStringLiteral("capturing") &&
        captureState_ != QStringLiteral("connecting")) {
        return;
    }

    automaticStopPending_ = false;
    awaitFreshSignal_ = false;
    startThresholdWaitingLogged_ = false;
    autoRecordController_.reset();
    armed_ = false;
    emit armedChanged();
    setCaptureState(QStringLiteral("stopping"));
    emit stopCaptureRequested();
}

void CaptureViewModel::stopAutomaticRecording() {
    if (captureState_ != QStringLiteral("capturing") &&
        captureState_ != QStringLiteral("connecting")) {
        return;
    }
    automaticStopPending_ = true;
    setCaptureState(QStringLiteral("stopping"));
    emit stopCaptureRequested();
}

bool CaptureViewModel::completeAutomaticRecording() {
    if (!automaticStopPending_) {
        return false;
    }
    automaticStopPending_ = false;
    awaitFreshSignal_ = true;
    startThresholdWaitingLogged_ = false;
    resetDuration();
    setCaptureState(QStringLiteral("armed_waiting"));
    setIntegrityStatus(QStringLiteral("Armed; waiting for signal."));
    return true;
}

void CaptureViewModel::armCapture() {
    if (armed_) {
        return;
    }
    autoRecordController_.reset();
    beginCaptureSession();
    armed_ = true;
    awaitFreshSignal_ = true;
    startThresholdWaitingLogged_ = false;
    emit armedChanged();
    setCaptureState(QStringLiteral("armed_waiting"));
    setIntegrityStatus(QStringLiteral("Armed; waiting for manual or automatic start."));
}

void CaptureViewModel::disarmCapture() {
    if (!armed_) {
        return;
    }
    autoRecordController_.reset();
    armed_ = false;
    clearCaptureSession();
    awaitFreshSignal_ = false;
    startThresholdWaitingLogged_ = false;
    emit armedChanged();
    if (captureState_ == QStringLiteral("idle")) {
        setIntegrityStatus(QStringLiteral("Ready"));
    }
    if (captureState_ == QStringLiteral("armed_waiting")) {
        setCaptureState(QStringLiteral("idle"));
    }
}

void CaptureViewModel::feedAudioLevel(const float peakLinear,
                                      const bool newAudioGeneration) {
    if (!armed_ && autoRecordController_.state() == AutoRecordController::Idle) {
        return;
    }
    if (awaitFreshSignal_) {
        if (!newAudioGeneration) {
            return;
        }
        const float peakDbfs = peakLinear > 0.0F && !std::isnan(peakLinear)
                                   ? 20.0F * std::log10(peakLinear)
                                   : -std::numeric_limits<float>::infinity();
        const float thresholdDbfs = autoRecordController_.startThreshold();
        awaitFreshSignal_ = false;
        std::fprintf(stderr,
                     "[abracapture] automation: fresh generation accepted, level=%g "
                     "threshold=%g\n",
                     static_cast<double>(peakDbfs),
                     static_cast<double>(thresholdDbfs));
        if (peakDbfs < thresholdDbfs) {
            if (!startThresholdWaitingLogged_) {
                std::fprintf(stderr,
                             "[abracapture] automation: signal below start threshold -> "
                             "remain armed_waiting\n");
                startThresholdWaitingLogged_ = true;
            }
            return;
        }
    }
    autoRecordController_.feedLevel(peakLinear);
}

void CaptureViewModel::reportCaptureError(const QString& message) {
    durationTimer_->stop();
    setCaptureState(QStringLiteral("idle"));
    setIntegrityStatus(message.isEmpty()
                           ? QStringLiteral("Capture engine reported an error.")
                           : message);
}

void CaptureViewModel::handleSessionState(const abracapture::audio::SessionState state) {
    using abracapture::audio::SessionState;

    switch (state) {
    case SessionState::Idle:
    case SessionState::Stopped: {
        // An automatic recording stop tears down only the recording stream;
        // the monitor is restarted and the automation session remains armed.
        // Its queued Stopped notification must not overwrite armed_waiting.
        if (armed_ && captureState_ == QStringLiteral("armed_waiting")) {
            return;
        }
        const double finalDuration = durationSeconds_;
        const QString savedPath = outputPath_;
        const bool wasCapturing = (captureStartedAtMs_ != 0);
        durationTimer_->stop();
        setCaptureState(QStringLiteral("idle"));
        if (integrityStatus_ == QStringLiteral("Connecting to source…")) {
            setIntegrityStatus(QStringLiteral("Ready"));
        }
        if (wasCapturing) {
            auto* const service = findChild<NotificationService*>();
            if (service != nullptr) {
                QMetaObject::invokeMethod(
                    service,
                    "onCaptureStopped",
                    Qt::QueuedConnection,
                    Q_ARG(QString, savedPath),
                    Q_ARG(double, finalDuration));
            }
        }
        // A terminal state is handled once.  Leaving the start timestamp set
        // would make every subsequent duplicate Stopped callback look like a
        // fresh recording and flood desktop notifications.
        captureStartedAtMs_ = 0;
        break;
    }
    case SessionState::Connecting:
    case SessionState::Reconnecting:
        setCaptureState(QStringLiteral("connecting"));
        setIntegrityStatus(state == SessionState::Reconnecting
                               ? QStringLiteral("Reconnecting to source…")
                               : QStringLiteral("Connecting to source…"));
        break;
    case SessionState::Capturing:
        if (captureStartedAtMs_ == 0) {
            captureStartedAtMs_ = QDateTime::currentMSecsSinceEpoch();
            durationSeconds_ = 0.0;
            emit durationSecondsChanged();
        }
        durationTimer_->start();
        setCaptureState(QStringLiteral("capturing"));
        setIntegrityStatus(QStringLiteral("Signal is being captured"));
        break;
    case SessionState::Error: {
        durationTimer_->stop();
        captureStartedAtMs_ = 0;
        setCaptureState(QStringLiteral("idle"));
        const QString errorMsg = QStringLiteral("Capture engine reported an error.");
        setIntegrityStatus(errorMsg);
        auto* const service = findChild<NotificationService*>();
        if (service != nullptr) {
            QMetaObject::invokeMethod(
                service,
                "onCaptureError",
                Qt::QueuedConnection,
                Q_ARG(QString, errorMsg));
        }
        break;
    }
    }
}

void CaptureViewModel::updateEngineFormat(const QString& description,
                                          const CaptureKind kind) {
    if (formatDescription_ != description) {
        formatDescription_ = description;
        emit formatDescriptionChanged();
    }
    if (captureKind_ != kind) {
        captureKind_ = kind;
        emit captureKindChanged();
    }
}

void CaptureViewModel::updateIntegrityMetrics(const qulonglong overflowCount) {
    if (captureState_ != QStringLiteral("capturing")) {
        return;
    }
    setIntegrityStatus(overflowCount == 0
                           ? QStringLiteral("Clean capture — no buffer overruns")
                           : QStringLiteral("%1 buffer overrun frame(s) detected")
                                 .arg(overflowCount));
}

void CaptureViewModel::updateDuration() {
    if (captureStartedAtMs_ == 0) {
        return;
    }
    const double nextDuration = static_cast<double>(
                                    QDateTime::currentMSecsSinceEpoch() - captureStartedAtMs_) /
                                1000.0;
    if (!qFuzzyCompare(durationSeconds_ + 1.0, nextDuration + 1.0)) {
        durationSeconds_ = nextDuration;
        emit durationSecondsChanged();
    }
}

void CaptureViewModel::setCaptureState(const QString& state) {
    if (captureState_ == state) {
        return;
    }
    captureState_ = state;
    emit captureStateChanged();
}

void CaptureViewModel::setIntegrityStatus(const QString& status) {
    if (integrityStatus_ == status) {
        return;
    }
    integrityStatus_ = status;
    emit integrityStatusChanged();
}

void CaptureViewModel::resetDuration() {
    captureStartedAtMs_ = 0;
    if (!qFuzzyIsNull(durationSeconds_)) {
        durationSeconds_ = 0.0;
        emit durationSecondsChanged();
    }
}

void CaptureViewModel::beginCaptureSession() {
    const QString nextId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (captureSessionId_ == nextId) {
        return;
    }
    captureSessionId_ = nextId;
    emit captureSessionIdChanged();
}

void CaptureViewModel::clearCaptureSession() {
    if (captureSessionId_.isEmpty()) {
        return;
    }
    captureSessionId_.clear();
    emit captureSessionIdChanged();
}
