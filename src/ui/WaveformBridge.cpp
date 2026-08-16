#include "WaveformBridge.hpp"

#include <QMetaObject>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <utility>

WaveformBridge::WaveformBridge(QObject* const parent)
    : QObject(parent),
      refreshTimer_(new QTimer(this)) {
    qRegisterMetaType<abracapture::audio::CaptureMetricsSnapshot>();
    qRegisterMetaType<QVector<float>>();

    refreshTimer_->setInterval(RefreshIntervalMs);
    refreshTimer_->setTimerType(Qt::PreciseTimer);
    connect(refreshTimer_, &QTimer::timeout,
            this, &WaveformBridge::publishPendingSnapshot);
    refreshTimer_->start();
}

QVariantList WaveformBridge::waveformSamples() const {
    return toVariantList(waveformSamples_);
}

QVariantList WaveformBridge::peakLevels() const {
    return toVariantList(peakLevels_);
}

QVariantList WaveformBridge::rmsLevels() const {
    return toVariantList(rmsLevels_);
}

float WaveformBridge::peakLevel() const noexcept {
    if (peakLevels_.isEmpty()) {
        return 0.0F;
    }
    return *std::max_element(peakLevels_.cbegin(), peakLevels_.cend());
}

float WaveformBridge::rmsLevel() const noexcept {
    if (rmsLevels_.isEmpty()) {
        return 0.0F;
    }
    return *std::max_element(rmsLevels_.cbegin(), rmsLevels_.cend());
}

int WaveformBridge::channelCount() const noexcept {
    return std::max(waveformChannelCount_, static_cast<int>(peakLevels_.size()));
}

int WaveformBridge::snapshotWindowSize() const noexcept {
    return snapshotWindowSize_;
}

qulonglong WaveformBridge::framesCaptured() const noexcept {
    return framesCaptured_;
}

qulonglong WaveformBridge::overflowCount() const noexcept {
    return overflowCount_;
}

void WaveformBridge::setSnapshotWindowSize(const int sampleCount) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, sampleCount] { setSnapshotWindowSize(sampleCount); },
            Qt::QueuedConnection);
        return;
    }

    const int boundedCount = std::clamp(sampleCount, 1, MaximumSnapshotWindowSize);
    if (snapshotWindowSize_ == boundedCount) {
        return;
    }

    snapshotWindowSize_ = boundedCount;
    trimToWindow(waveformSamples_);
    trimToWindow(pendingWaveformSamples_);
    emit snapshotWindowSizeChanged();
    emit waveformSamplesChanged();
}

void WaveformBridge::submitVisualizationSnapshot(QVector<float> samples) {
    submitInterleavedVisualizationSnapshot(std::move(samples), 1);
}

void WaveformBridge::submitInterleavedVisualizationSnapshot(
    QVector<float> samples,
    const int channelCount) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, samples = std::move(samples), channelCount]() mutable {
                submitInterleavedVisualizationSnapshot(std::move(samples), channelCount);
            },
            Qt::QueuedConnection);
        return;
    }

    const int safeChannelCount = std::clamp(channelCount, 1, MaximumMeterChannels);
    const int completeFrameCount = static_cast<int>(samples.size()) / safeChannelCount;
    const int outputFrameCount = std::min(completeFrameCount, snapshotWindowSize_);
    const int firstFrame = completeFrameCount - outputFrameCount;

    QVector<float> downmixedSamples;
    downmixedSamples.reserve(outputFrameCount);
    for (int frame = firstFrame; frame < completeFrameCount; ++frame) {
        float sum = 0.0F;
        const int frameOffset = frame * safeChannelCount;
        for (int channel = 0; channel < safeChannelCount; ++channel) {
            const float sample = samples.at(frameOffset + channel);
            sum += std::isfinite(sample) ? std::clamp(sample, -1.0F, 1.0F) : 0.0F;
        }
        downmixedSamples.append(sum / static_cast<float>(safeChannelCount));
    }

    pendingWaveformSamples_ = std::move(downmixedSamples);
    pendingWaveformChannelCount_ = safeChannelCount;
    waveformPending_ = true;
}

void WaveformBridge::submitMetricsSnapshot(
    const abracapture::audio::CaptureMetricsSnapshot snapshot) {
    submitChannelMetricsSnapshot(snapshot, 0);
}

void WaveformBridge::submitChannelMetricsSnapshot(
    const abracapture::audio::CaptureMetricsSnapshot snapshot,
    const int channel) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, snapshot, channel] { submitChannelMetricsSnapshot(snapshot, channel); },
            Qt::QueuedConnection);
        return;
    }

    if (channel < 0 || channel >= MaximumMeterChannels) {
        return;
    }

    if (pendingPeakLevels_.size() <= channel) {
        pendingPeakLevels_.resize(channel + 1);
        pendingRmsLevels_.resize(channel + 1);
    }
    pendingPeakLevels_[channel] = boundedLevel(snapshot.peakLevel);
    pendingRmsLevels_[channel] = boundedLevel(snapshot.rmsLevel);
    pendingFramesCaptured_ = static_cast<qulonglong>(snapshot.framesCaptured);
    pendingOverflowCount_ = static_cast<qulonglong>(snapshot.overflowCount);
    metricsPending_ = true;
}

void WaveformBridge::publishPendingSnapshot() {
    const int previousChannelCount = channelCount();

    if (waveformPending_) {
        waveformSamples_ = std::move(pendingWaveformSamples_);
        waveformChannelCount_ = pendingWaveformChannelCount_;
        waveformPending_ = false;
        emit waveformSamplesChanged();
    }

    if (metricsPending_) {
        const bool levelsChanged = peakLevels_ != pendingPeakLevels_ ||
                                   rmsLevels_ != pendingRmsLevels_;
        const bool integrityChanged = framesCaptured_ != pendingFramesCaptured_ ||
                                      overflowCount_ != pendingOverflowCount_;
        peakLevels_ = pendingPeakLevels_;
        rmsLevels_ = pendingRmsLevels_;
        framesCaptured_ = pendingFramesCaptured_;
        overflowCount_ = pendingOverflowCount_;
        metricsPending_ = false;

        if (levelsChanged) {
            emit this->levelsChanged();
        }
        if (integrityChanged) {
            emit metricsChanged();
        }
    }

    if (channelCount() != previousChannelCount) {
        emit channelCountChanged();
    }
}

float WaveformBridge::boundedLevel(const float level) noexcept {
    return std::isfinite(level) ? std::clamp(level, 0.0F, 1.0F) : 0.0F;
}

QVariantList WaveformBridge::toVariantList(const QVector<float>& values) {
    QVariantList result;
    result.reserve(values.size());
    for (const float value : values) {
        result.append(value);
    }
    return result;
}

void WaveformBridge::trimToWindow(QVector<float>& samples) const {
    if (samples.size() <= snapshotWindowSize_) {
        return;
    }
    samples = samples.mid(samples.size() - snapshotWindowSize_);
}
