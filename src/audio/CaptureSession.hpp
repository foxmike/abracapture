#pragma once

#include "audio/AudioRingBuffer.hpp"
#include "audio/CaptureMetrics.hpp"
#include "audio/PipeWireCaptureStream.hpp"
#include "pipewire/SourceCatalog.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

struct pw_core;
struct pw_thread_loop;

namespace abracapture::audio {

enum class SessionState : std::uint8_t {
    Idle,
    Connecting,
    Capturing,
    Reconnecting,
    Stopped,
    Error,
};

class CaptureSession {
public:
    using StateCallback = std::function<void(SessionState)>;
    using ConsumerCallback = std::function<void(const float*, std::size_t)>;
    using CaptureVisualizationSnapshot =
        PipeWireCaptureStream::CaptureVisualizationSnapshot;

    CaptureSession(pw_thread_loop* threadLoop,
                   pw_core* core,
                   SourceCatalog& sourceCatalog,
                   std::size_t ringBufferCapacity = 65536,
                   StateCallback stateCallback = {},
                   ConsumerCallback consumerCallback = {});
    ~CaptureSession();

    CaptureSession(const CaptureSession&) = delete;
    CaptureSession& operator=(const CaptureSession&) = delete;
    CaptureSession(CaptureSession&&) = delete;
    CaptureSession& operator=(CaptureSession&&) = delete;

    [[nodiscard]] bool begin(std::uint32_t sourceId,
                             spa_audio_format format,
                             std::uint32_t rate,
                             std::uint32_t channels);
    void stop() noexcept;
    void drainPendingSamples() noexcept;

    [[nodiscard]] SessionState state() const noexcept;
    void setMaxReconnectAttempts(std::size_t attempts) noexcept;

    [[nodiscard]] spa_audio_format negotiatedFormat() const noexcept;
    [[nodiscard]] std::uint32_t negotiatedSampleRate() const noexcept;
    [[nodiscard]] std::uint32_t negotiatedChannels() const noexcept;
    [[nodiscard]] bool isNativeCapture() const noexcept;
    [[nodiscard]] CaptureMetricsSnapshot metricsSnapshot() const noexcept;
    [[nodiscard]] bool visualizationSnapshot(
        CaptureVisualizationSnapshot& snapshot) const noexcept;

private:
    void handleStreamStatus(PipeWireCaptureStream::StreamStatus status) noexcept;
    void transitionTo(SessionState next) noexcept;
    void reconnectLoop();
    [[nodiscard]] std::optional<SourceDescriptor> resolveSource() const;
    [[nodiscard]] std::unique_ptr<PipeWireCaptureStream> makeStream(
        std::uint32_t nodeId);
    void flushRingBuffer() noexcept;

    pw_thread_loop* const threadLoop_;
    pw_core* const core_;
    SourceCatalog& sourceCatalog_;

    AudioRingBuffer ringBuffer_;
    CaptureMetrics metrics_;
    mutable std::mutex streamMutex_;
    std::unique_ptr<PipeWireCaptureStream> stream_;

    SourceDescriptor stableSource_{};
    spa_audio_format requestedFormat_ = SPA_AUDIO_FORMAT_UNKNOWN;
    std::uint32_t requestedRate_ = 0;
    std::uint32_t requestedChannels_ = 0;

    const StateCallback stateCallback_;
    const ConsumerCallback consumerCallback_;
    std::atomic<SessionState> state_{SessionState::Idle};
    std::atomic<std::size_t> maxReconnectAttempts_{5};
    const std::chrono::milliseconds reconnectBackoff_{250};

    std::mutex reconnectMutex_;
    std::condition_variable reconnectCondition_;
    bool reconnectRequested_ = false;
    bool workerExit_ = false;
    std::atomic_bool stopRequested_{true};
    std::atomic_bool suppressStreamStatus_{false};
    std::atomic_bool reconnectAttemptActive_{false};
    std::atomic<std::uint64_t> lifecycleGeneration_{0};
    std::thread reconnectWorker_;
};

} // namespace abracapture::audio
