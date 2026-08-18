#include "audio/CaptureSession.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include <limits>
#include <utility>

namespace abracapture::audio {
namespace {

bool isSameStableSource(const SourceDescriptor& expected,
                        const SourceDescriptor& candidate) {
    if (expected.mediaClass != candidate.mediaClass ||
        expected.category != candidate.category) {
        return false;
    }

    if (expected.deviceId && candidate.deviceId &&
        expected.deviceId == candidate.deviceId) {
        return true;
    }
    if (!expected.nodeName.empty() &&
        expected.nodeName == candidate.nodeName) {
        return true;
    }
    if (!expected.displayName.empty() &&
        expected.displayName == candidate.displayName) {
        return expected.appName.empty() || candidate.appName.empty() ||
               expected.appName == candidate.appName;
    }
    return false;
}

} // namespace

CaptureSession::CaptureSession(pw_thread_loop* const threadLoop,
                               pw_core* const core,
                               SourceCatalog& sourceCatalog,
                               const std::size_t ringBufferCapacity,
                               StateCallback stateCallback,
                               ConsumerCallback consumerCallback)
    : threadLoop_(threadLoop),
      core_(core),
      sourceCatalog_(sourceCatalog),
      ringBuffer_(ringBufferCapacity),
      stateCallback_(std::move(stateCallback)),
      consumerCallback_(std::move(consumerCallback)),
      reconnectWorker_(&CaptureSession::reconnectLoop, this) {}

CaptureSession::~CaptureSession() {
    stop();
    {
        std::lock_guard lock(reconnectMutex_);
        workerExit_ = true;
    }
    reconnectCondition_.notify_one();
    if (reconnectWorker_.joinable()) {
        reconnectWorker_.join();
    }
}

bool CaptureSession::begin(const std::uint32_t sourceId,
                           const spa_audio_format format,
                           const std::uint32_t rate,
                           const std::uint32_t channels,
                           std::string targetObjectSerial) {
    lifecycleGeneration_.fetch_add(1, std::memory_order_acq_rel);
    stopRequested_.store(true, std::memory_order_release);
    {
        std::lock_guard reconnectLock(reconnectMutex_);
        reconnectRequested_ = false;
    }

    const auto source = sourceCatalog_.sourceById(sourceId);
    if (!source || threadLoop_ == nullptr || core_ == nullptr ||
        channels > SPA_AUDIO_MAX_CHANNELS) {
        std::fprintf(stderr,
                     "[abracapture] monitor rebind failed: source unavailable node=%u\n",
                     sourceId);
        transitionTo(SessionState::Error);
        return false;
    }

    {
        std::lock_guard streamLock(streamMutex_);
        if (stream_) {
            std::fprintf(stderr,
                         "[abracapture] old monitor/capture binding stopped serial=%s\n",
                         requestedObjectSerial_.c_str());
            stream_->stop();
            stream_.reset();
        }
        ringBuffer_.reset();
        stableSource_ = *source;
        requestedFormat_ = format;
        requestedRate_ = rate;
        requestedChannels_ = channels;
        requestedObjectSerial_ = targetObjectSerial.empty()
                                     ? source->objectSerial
                                     : std::move(targetObjectSerial);
        activeChannels_.store(0, std::memory_order_release);
        if (source->sourceFormat) {
            stream_ = makeStream(*source);
        }
    }

    if (!source->sourceFormat) {
        std::fprintf(stderr,
                     "[abracapture] monitor rebind pending format node=%u serial=%s\n",
                     source->nodeId,
                     requestedObjectSerial_.c_str());
        stopRequested_.store(false, std::memory_order_release);
        transitionTo(SessionState::Connecting);
        {
            std::lock_guard reconnectLock(reconnectMutex_);
            reconnectRequested_ = true;
        }
        reconnectCondition_.notify_one();
        return true;
    }

    fprintf(stderr,
            "[abracapture] source selection: target=node=%u serial=%s node.name=%s app=%s\n",
            source->nodeId,
            requestedObjectSerial_.c_str(),
            source->nodeName.c_str(),
            source->appName.c_str());

    stopRequested_.store(false, std::memory_order_release);
    transitionTo(SessionState::Connecting);

    bool started = false;
    {
        std::lock_guard streamLock(streamMutex_);
        started = stream_ && stream_->start();
    }
    if (!started) {
        handleStreamStatus(PipeWireCaptureStream::StreamStatus::Error);
    }
    return true;
}

void CaptureSession::stop() noexcept {
    lifecycleGeneration_.fetch_add(1, std::memory_order_acq_rel);
    stopRequested_.store(true, std::memory_order_release);
    {
        std::lock_guard reconnectLock(reconnectMutex_);
        reconnectRequested_ = false;
    }
    reconnectCondition_.notify_one();

    {
        std::lock_guard streamLock(streamMutex_);
        if (stream_) {
            activeChannels_.store(stream_->negotiatedChannels(), std::memory_order_release);
            stream_->stop();
            stream_.reset();
        }
    }
    flushRingBuffer();
    transitionTo(SessionState::Stopped);
}

void CaptureSession::drainPendingSamples() noexcept {
    flushRingBuffer();
}

SessionState CaptureSession::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

void CaptureSession::setMaxReconnectAttempts(const std::size_t attempts) noexcept {
    maxReconnectAttempts_.store(attempts, std::memory_order_release);
}

spa_audio_format CaptureSession::negotiatedFormat() const noexcept {
    std::lock_guard lock(streamMutex_);
    return stream_ ? stream_->negotiatedFormat() : SPA_AUDIO_FORMAT_UNKNOWN;
}

std::uint32_t CaptureSession::negotiatedSampleRate() const noexcept {
    std::lock_guard lock(streamMutex_);
    return stream_ ? stream_->negotiatedSampleRate() : 0;
}

std::uint32_t CaptureSession::negotiatedChannels() const noexcept {
    std::lock_guard lock(streamMutex_);
    return stream_ ? stream_->negotiatedChannels() : 0;
}

bool CaptureSession::isNativeCapture() const noexcept {
    std::lock_guard lock(streamMutex_);
    return stream_ && stream_->isNativeCapture();
}

CaptureMetricsSnapshot CaptureSession::metricsSnapshot() const noexcept {
    return metrics_.snapshot();
}

bool CaptureSession::visualizationSnapshot(
    CaptureVisualizationSnapshot& snapshot) const noexcept {
    std::lock_guard lock(streamMutex_);
    return stream_ && stream_->visualizationSnapshot(snapshot);
}

void CaptureSession::handleStreamStatus(
    const PipeWireCaptureStream::StreamStatus status) noexcept {
    if (stopRequested_.load(std::memory_order_acquire) ||
        suppressStreamStatus_.load(std::memory_order_acquire)) {
        return;
    }

    switch (status) {
    case PipeWireCaptureStream::StreamStatus::Connected:
    case PipeWireCaptureStream::StreamStatus::FormatRenegotiated:
        transitionTo(SessionState::Capturing);
        break;
    case PipeWireCaptureStream::StreamStatus::Connecting:
        if (state() != SessionState::Reconnecting) {
            transitionTo(SessionState::Connecting);
        }
        break;
    case PipeWireCaptureStream::StreamStatus::Disconnected:
    case PipeWireCaptureStream::StreamStatus::Error: {
        transitionTo(SessionState::Reconnecting);
        if (!reconnectAttemptActive_.load(std::memory_order_acquire)) {
            {
                std::lock_guard lock(reconnectMutex_);
                reconnectRequested_ = true;
            }
            reconnectCondition_.notify_one();
        }
        break;
    }
    }
}

void CaptureSession::transitionTo(const SessionState next) noexcept {
    const SessionState previous = state_.exchange(next, std::memory_order_acq_rel);
    if (previous != next && stateCallback_) {
        try {
            stateCallback_(next);
        } catch (...) {
            // State notifications must not unwind through a PipeWire callback.
        }
    }
}

void CaptureSession::reconnectLoop() {
    for (;;) {
        {
            std::unique_lock lock(reconnectMutex_);
            reconnectCondition_.wait(lock, [this] {
                return reconnectRequested_ || workerExit_;
            });
            if (workerExit_) {
                return;
            }
            reconnectRequested_ = false;
        }

        const std::uint64_t generation =
            lifecycleGeneration_.load(std::memory_order_acquire);
        const std::size_t attemptLimit =
            maxReconnectAttempts_.load(std::memory_order_acquire);
        bool reconnected = false;
        reconnectAttemptActive_.store(true, std::memory_order_release);
        for (std::size_t attempt = 0;
             attempt < attemptLimit &&
             !stopRequested_.load(std::memory_order_acquire) &&
             lifecycleGeneration_.load(std::memory_order_acquire) == generation;
             ++attempt) {
            const auto delay = reconnectBackoff_ * static_cast<int>(attempt + 1);
            {
                std::unique_lock lock(reconnectMutex_);
                if (reconnectCondition_.wait_for(lock, delay, [this, generation] {
                        return workerExit_ ||
                               stopRequested_.load(std::memory_order_acquire) ||
                               lifecycleGeneration_.load(std::memory_order_acquire) != generation;
                    })) {
                    break;
                }
            }

            const auto source = resolveSource();
            if (!source) {
                continue;
            }

            std::lock_guard streamLock(streamMutex_);
            if (stopRequested_.load(std::memory_order_acquire) ||
                lifecycleGeneration_.load(std::memory_order_acquire) != generation) {
                break;
            }
            // The numeric node ID may be reused for a new PipeWire object.
            // Refresh both the stable descriptor and the concrete serial
            // before constructing the replacement stream.
            stableSource_ = *source;
            requestedObjectSerial_ = source->objectSerial;
            std::fprintf(stderr,
                         "[abracapture] source selection: target=node=%u serial=%s\n",
                         source->nodeId,
                         requestedObjectSerial_.c_str());
            suppressStreamStatus_.store(true, std::memory_order_release);
            if (stream_) {
                stream_->stop();
                stream_.reset();
            }
            stream_ = makeStream(*source);
            suppressStreamStatus_.store(false, std::memory_order_release);
            if (stream_ && stream_->start() &&
                stream_->status() != PipeWireCaptureStream::StreamStatus::Error &&
                stream_->status() != PipeWireCaptureStream::StreamStatus::Disconnected) {
                reconnected = true;
                break;
            }
        }
        reconnectAttemptActive_.store(false, std::memory_order_release);

        if (!reconnected && !stopRequested_.load(std::memory_order_acquire) &&
            lifecycleGeneration_.load(std::memory_order_acquire) == generation) {
            transitionTo(SessionState::Error);
        }
    }
}

std::optional<SourceDescriptor> CaptureSession::resolveSource() const {
    SourceDescriptor expected;
    {
        std::lock_guard lock(streamMutex_);
        expected = stableSource_;
    }
    const auto& sources = sourceCatalog_.sources();
    const auto sameLogical = [&expected](const SourceDescriptor& source) {
        return !expected.logicalIdentity.empty() &&
               source.logicalIdentity == expected.logicalIdentity;
    };
    const auto usable = [](const SourceDescriptor& source) {
        return source.sourceFormat.has_value();
    };
    const auto exactNode = std::find_if(sources.begin(), sources.end(),
                                        [&expected, &sameLogical, &usable](const SourceDescriptor& source) {
                                            return usable(source) && sameLogical(source) &&
                                                   source.nodeId == expected.nodeId;
                                        });
    if (exactNode != sources.end()) {
        return *exactNode;
    }
    const auto exactInstance = std::find_if(sources.begin(), sources.end(),
                                            [&expected, &usable](const SourceDescriptor& source) {
                                                return usable(source) &&
                                                       !expected.instanceIdentity.empty() &&
                                                       source.instanceIdentity == expected.instanceIdentity;
                                            });
    if (exactInstance != sources.end()) {
        return *exactInstance;
    }
    const auto logicalBegin = std::find_if(sources.begin(), sources.end(),
                                           [&sameLogical, &usable](const SourceDescriptor& source) {
                                               return usable(source) && sameLogical(source);
                                           });
    if (logicalBegin != sources.end()) {
        const auto logicalNext = std::find_if(
            std::next(logicalBegin), sources.end(),
            [&sameLogical, &usable](const SourceDescriptor& source) {
                return usable(source) && sameLogical(source);
            });
        if (logicalNext == sources.end()) {
            return *logicalBegin;
        }
        return std::nullopt;
    }
    const auto match = std::find_if(sources.begin(), sources.end(),
                                    [&expected, &usable](const SourceDescriptor& source) {
                                        return usable(source) && isSameStableSource(expected, source);
                                    });
    return match == sources.end() ? std::nullopt : std::optional<SourceDescriptor>(*match);
}

std::unique_ptr<PipeWireCaptureStream> CaptureSession::makeStream(
    const SourceDescriptor& source) {
    spa_audio_info_raw requested = *source.sourceFormat;
    SourceDescriptor target = source;
    if (!requestedObjectSerial_.empty()) {
        target.objectSerial = requestedObjectSerial_;
    }
    // A non-zero rate is an explicit user conversion request. Otherwise the
    // current source node's rate, channels, sample representation and layout
    // are proposed unchanged.
    if (requestedRate_ != 0) {
        requested.rate = requestedRate_;
    }
    return std::make_unique<PipeWireCaptureStream>(
        threadLoop_, core_, ringBuffer_, source.nodeId, target.objectSerial,
        *source.sourceFormat, requested,
        [this](const PipeWireCaptureStream::StreamStatus status) {
            handleStreamStatus(status);
        });
}

void CaptureSession::flushRingBuffer() noexcept {
    std::size_t channels = activeChannels_.load(std::memory_order_acquire);
    if (channels == 0) {
        std::lock_guard streamLock(streamMutex_);
        if (stream_) {
            channels = stream_->negotiatedChannels();
            activeChannels_.store(static_cast<std::uint32_t>(channels), std::memory_order_release);
        }
    }
    if (channels == 0) {
        return;
    }
    std::array<float, 4096 * SPA_AUDIO_MAX_CHANNELS> samples{};
    while (ringBuffer_.available() != 0) {
        const std::size_t availableSamples = ringBuffer_.available();
        const std::size_t sampleCapacity = std::min(samples.size(), availableSamples);
        const std::size_t samplesToRead = sampleCapacity - (sampleCapacity % channels);
        if (samplesToRead == 0) {
            break;
        }
        const std::size_t samplesRead = ringBuffer_.read(samples.data(), samplesToRead);
        const std::size_t frameCount = samplesRead / channels;
        if (frameCount == 0) {
            break;
        }
        if (consumerCallback_) {
            try {
                consumerCallback_(samples.data(), frameCount);
            } catch (...) {
                break;
            }
        }
    }
}

} // namespace abracapture::audio
