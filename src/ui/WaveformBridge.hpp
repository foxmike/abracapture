#pragma once

#include "audio/CaptureMetrics.hpp"

#include <QObject>
#include <QVariantList>
#include <QVector>
#include <QtGlobal>

class QTimer;

class WaveformBridge final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList waveformSamples READ waveformSamples NOTIFY waveformSamplesChanged)
    Q_PROPERTY(QVariantList peakLevels READ peakLevels NOTIFY levelsChanged)
    Q_PROPERTY(QVariantList rmsLevels READ rmsLevels NOTIFY levelsChanged)
    Q_PROPERTY(float peakLevel READ peakLevel NOTIFY levelsChanged)
    Q_PROPERTY(float rmsLevel READ rmsLevel NOTIFY levelsChanged)
    Q_PROPERTY(int channelCount READ channelCount NOTIFY channelCountChanged)
    Q_PROPERTY(int snapshotWindowSize READ snapshotWindowSize WRITE setSnapshotWindowSize
                   NOTIFY snapshotWindowSizeChanged)
    Q_PROPERTY(qulonglong framesCaptured READ framesCaptured NOTIFY metricsChanged)
    Q_PROPERTY(qulonglong overflowCount READ overflowCount NOTIFY metricsChanged)

public:
    explicit WaveformBridge(QObject* parent = nullptr);

    [[nodiscard]] QVariantList waveformSamples() const;
    [[nodiscard]] QVariantList peakLevels() const;
    [[nodiscard]] QVariantList rmsLevels() const;
    [[nodiscard]] float peakLevel() const noexcept;
    [[nodiscard]] float rmsLevel() const noexcept;
    [[nodiscard]] int channelCount() const noexcept;
    [[nodiscard]] int snapshotWindowSize() const noexcept;
    [[nodiscard]] qulonglong framesCaptured() const noexcept;
    [[nodiscard]] qulonglong overflowCount() const noexcept;

    void setSnapshotWindowSize(int sampleCount);

public slots:
    // The caller must copy samples out of CaptureVisualizationSnapshot before
    // crossing this queued boundary. The bridge never reads capture storage.
    void submitVisualizationSnapshot(QVector<float> samples);
    void submitInterleavedVisualizationSnapshot(QVector<float> samples, int channelCount);

    // Metrics remain separate from waveform data. Supplying a channel index
    // allows future producers to publish per-channel meters without changing
    // the QML-facing API.
    void submitMetricsSnapshot(abracapture::audio::CaptureMetricsSnapshot snapshot);
    void submitChannelMetricsSnapshot(abracapture::audio::CaptureMetricsSnapshot snapshot,
                                      int channel);

signals:
    void waveformSamplesChanged();
    void levelsChanged();
    void channelCountChanged();
    void metricsChanged();
    void snapshotWindowSizeChanged();

private slots:
    void publishPendingSnapshot();

private:
    static constexpr int DefaultSnapshotWindowSize = 512;
    static constexpr int MaximumSnapshotWindowSize = 512;
    static constexpr int MaximumMeterChannels = 64;
    static constexpr int RefreshIntervalMs = 17;

    static float boundedLevel(float level) noexcept;
    static QVariantList toVariantList(const QVector<float>& values);
    void trimToWindow(QVector<float>& samples) const;

    QTimer* refreshTimer_ = nullptr;
    QVector<float> waveformSamples_;
    QVector<float> pendingWaveformSamples_;
    QVector<float> peakLevels_;
    QVector<float> rmsLevels_;
    QVector<float> pendingPeakLevels_;
    QVector<float> pendingRmsLevels_;
    int snapshotWindowSize_ = DefaultSnapshotWindowSize;
    int waveformChannelCount_ = 1;
    int pendingWaveformChannelCount_ = 1;
    qulonglong framesCaptured_ = 0;
    qulonglong overflowCount_ = 0;
    qulonglong pendingFramesCaptured_ = 0;
    qulonglong pendingOverflowCount_ = 0;
    bool waveformPending_ = false;
    bool metricsPending_ = false;
};

Q_DECLARE_METATYPE(abracapture::audio::CaptureMetricsSnapshot)
