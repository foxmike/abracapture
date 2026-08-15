#pragma once

#include "audio/AudioRingBuffer.hpp"

#include <spa/param/audio/raw.h>
#include <spa/utils/hook.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>

struct pw_core;
struct pw_stream;
struct pw_thread_loop;
struct spa_pod;

namespace abracapture::audio {

class PipeWireCaptureStream {
public:
    static constexpr std::size_t VisualizationSampleCapacity = 512;

    struct CaptureVisualizationSnapshot {
        std::array<float, VisualizationSampleCapacity> samples{};
        std::size_t sampleCount = 0;
        float peak = 0.0F;
        float rms = 0.0F;
        std::uint64_t sequence = 0;
    };

    enum class StreamStatus : std::uint8_t {
        Disconnected,
        Connecting,
        Connected,
        Error,
        FormatRenegotiated,
    };

    using StatusCallback = std::function<void(StreamStatus)>;

    // threadLoop and core are non-owning Phase-B handles and must outlive this object.
    // AudioRingBuffer stores interleaved float samples (one slot per channel sample).
    PipeWireCaptureStream(pw_thread_loop* threadLoop,
                          pw_core* core,
                          AudioRingBuffer& ringBuffer,
                          std::uint32_t targetNodeId,
                          spa_audio_format desiredFormat,
                          std::uint32_t desiredSampleRate,
                          std::uint32_t desiredChannels,
                          StatusCallback statusCallback = {});
    ~PipeWireCaptureStream();

    PipeWireCaptureStream(const PipeWireCaptureStream&) = delete;
    PipeWireCaptureStream& operator=(const PipeWireCaptureStream&) = delete;
    PipeWireCaptureStream(PipeWireCaptureStream&&) = delete;
    PipeWireCaptureStream& operator=(PipeWireCaptureStream&&) = delete;

    [[nodiscard]] bool start();
    void stop() noexcept;
    void destroy() noexcept;

    [[nodiscard]] spa_audio_format negotiatedFormat() const noexcept;
    [[nodiscard]] std::uint32_t negotiatedSampleRate() const noexcept;
    [[nodiscard]] std::uint32_t negotiatedChannels() const noexcept;
    [[nodiscard]] bool isNativeCapture() const noexcept;
    [[nodiscard]] StreamStatus status() const noexcept;

    // This is a copy from independent publisher storage; it never reads or drains
    // the capture ring buffer. False means that no snapshot has been published yet.
    [[nodiscard]] bool visualizationSnapshot(CaptureVisualizationSnapshot& snapshot) const noexcept;

private:
    struct VisualizationSlot {
        std::array<std::atomic<float>, VisualizationSampleCapacity> samples{};
        std::atomic<std::size_t> sampleCount{0};
        std::atomic<float> peak{0.0F};
        std::atomic<float> rms{0.0F};
        std::atomic<std::uint64_t> version{0};
    };

    static void on_process(void* data) noexcept;
    static void onParamChanged(void* data, std::uint32_t id, const spa_pod* param) noexcept;
    static void onStateChanged(void* data, int oldState, int state, const char* error) noexcept;

    void process() noexcept;
    void handleParamChanged(std::uint32_t id, const spa_pod* param) noexcept;
    void handleStateChanged(int state) noexcept;
    void setStatus(StreamStatus status) noexcept;
    void publishVisualization(const float* samples, std::size_t sampleCount) noexcept;

    pw_thread_loop* threadLoop_ = nullptr;
    pw_core* core_ = nullptr;
    AudioRingBuffer* ringBuffer_ = nullptr;
    pw_stream* stream_ = nullptr;
    spa_hook streamListener_{};
    bool streamListenerInstalled_ = false;

    const std::uint32_t targetNodeId_;
    const spa_audio_format desiredFormat_;
    const std::uint32_t desiredSampleRate_;
    const std::uint32_t desiredChannels_;
    const StatusCallback statusCallback_;

    std::atomic<spa_audio_format> negotiatedFormat_{SPA_AUDIO_FORMAT_UNKNOWN};
    std::atomic<std::uint32_t> negotiatedSampleRate_{0};
    std::atomic<std::uint32_t> negotiatedChannels_{0};
    std::atomic<StreamStatus> status_{StreamStatus::Disconnected};

    std::array<VisualizationSlot, 2> visualizationSlots_{};
    std::atomic<std::uint64_t> visualizationSequence_{0};
};

} // namespace abracapture::audio
