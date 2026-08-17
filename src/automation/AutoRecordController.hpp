#pragma once

#include <QElapsedTimer>
#include <QObject>

class AutoRecordController : public QObject {
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(float startThreshold READ startThreshold WRITE setStartThreshold)
    Q_PROPERTY(float stopThreshold READ stopThreshold WRITE setStopThreshold)
    Q_PROPERTY(int holdTimeMs READ holdTimeMs WRITE setHoldTimeMs)
    Q_PROPERTY(int preRollDurationMs READ preRollDurationMs WRITE setPreRollDurationMs)
    Q_PROPERTY(int postRollDurationMs READ postRollDurationMs WRITE setPostRollDurationMs)
    Q_PROPERTY(int silenceDurationMs READ silenceDurationMs WRITE setSilenceDurationMs)
    Q_PROPERTY(bool autoStartEnabled READ autoStartEnabled WRITE setAutoStartEnabled)
    Q_PROPERTY(bool autoStopEnabled READ autoStopEnabled WRITE setAutoStopEnabled)

public:
    enum State {
        Idle,
        Armed,
        Recording,
        PostRoll,
    };
    Q_ENUM(State)

    explicit AutoRecordController(QObject* parent = nullptr);

    [[nodiscard]] State state() const noexcept;

    // Thresholds are expressed in dBFS. Durations are expressed in milliseconds.
    [[nodiscard]] float startThreshold() const noexcept;
    void setStartThreshold(float thresholdDbfs) noexcept;

    [[nodiscard]] float stopThreshold() const noexcept;
    void setStopThreshold(float thresholdDbfs) noexcept;

    [[nodiscard]] int holdTimeMs() const noexcept;
    void setHoldTimeMs(int durationMs) noexcept;

    [[nodiscard]] int preRollDurationMs() const noexcept;
    void setPreRollDurationMs(int durationMs) noexcept;

    [[nodiscard]] int postRollDurationMs() const noexcept;
    void setPostRollDurationMs(int durationMs) noexcept;
    [[nodiscard]] int silenceDurationMs() const noexcept;
    void setSilenceDurationMs(int durationMs) noexcept;
    [[nodiscard]] bool autoStartEnabled() const noexcept;
    void setAutoStartEnabled(bool enabled) noexcept;
    [[nodiscard]] bool autoStopEnabled() const noexcept;
    void setAutoStopEnabled(bool enabled) noexcept;
    void reset() noexcept;

public slots:
    // Call from the GUI/metering thread, never from a realtime audio callback.
    void feedLevel(float peakLinear);

signals:
    void stateChanged(AutoRecordController::State state);
    void startRequested();
    void automaticStopRequested();
    void preRollRequested(int durationMs);

private:
    State state_ = Idle;
    float startThresholdDbfs_ = -30.0F;
    float stopThresholdDbfs_ = -40.0F;
    int holdTimeMs_ = 0;
    int preRollDurationMs_ = 0;
    int postRollDurationMs_ = 0;
    int silenceDurationMs_ = 3000;
    bool autoStartEnabled_ = false;
    bool autoStopEnabled_ = false;
    int consecutiveStartPeaks_ = 0;
    QElapsedTimer armedTimer_;
    QElapsedTimer postRollTimer_;
    QElapsedTimer silenceTimer_;
};
