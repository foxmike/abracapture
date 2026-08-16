#include "audio/PipeWireCaptureStream.hpp"

#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <pipewire/properties.h>
#include <pipewire/stream.h>
#include <pipewire/thread-loop.h>
#include <spa/buffer/buffer.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/param.h>
#include <spa/pod/builder.h>
#include <spa/utils/hook.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace abracapture::audio {
namespace {

constexpr std::size_t ConversionSampleCapacity = 2048;

static_assert(std::atomic<float>::is_always_lock_free,
              "visualization publication must remain lock-free");

[[nodiscard]] std::size_t bytesPerSample(const spa_audio_format format) noexcept {
    switch (format) {
    case SPA_AUDIO_FORMAT_S8:
    case SPA_AUDIO_FORMAT_U8:
    case SPA_AUDIO_FORMAT_S8P:
    case SPA_AUDIO_FORMAT_U8P:
        return 1;
    case SPA_AUDIO_FORMAT_S16:
    case SPA_AUDIO_FORMAT_S16P:
        return 2;
    case SPA_AUDIO_FORMAT_S24:
    case SPA_AUDIO_FORMAT_S24P:
        return 3;
    case SPA_AUDIO_FORMAT_S24_32:
    case SPA_AUDIO_FORMAT_S24_32P:
    case SPA_AUDIO_FORMAT_S32:
    case SPA_AUDIO_FORMAT_S32P:
    case SPA_AUDIO_FORMAT_F32:
    case SPA_AUDIO_FORMAT_F32P:
        return 4;
    case SPA_AUDIO_FORMAT_F64:
    case SPA_AUDIO_FORMAT_F64P:
        return 8;
    default:
        return 0;
    }
}

[[nodiscard]] bool isPlanar(const spa_audio_format format) noexcept {
    return SPA_AUDIO_FORMAT_IS_PLANAR(format);
}

template <typename Sample>
[[nodiscard]] Sample loadUnaligned(const std::byte* source) noexcept {
    Sample value{};
    std::memcpy(&value, source, sizeof(value));
    return value;
}

[[nodiscard]] float pcmToFloat(const std::byte* source,
                               const spa_audio_format format) noexcept {
    switch (format) {
    case SPA_AUDIO_FORMAT_F32:
    case SPA_AUDIO_FORMAT_F32P:
        return loadUnaligned<float>(source);
    case SPA_AUDIO_FORMAT_F64:
    case SPA_AUDIO_FORMAT_F64P:
        return static_cast<float>(loadUnaligned<double>(source));
    case SPA_AUDIO_FORMAT_S8:
    case SPA_AUDIO_FORMAT_S8P:
        return static_cast<float>(loadUnaligned<std::int8_t>(source)) / 128.0F;
    case SPA_AUDIO_FORMAT_U8:
    case SPA_AUDIO_FORMAT_U8P:
        return (static_cast<float>(loadUnaligned<std::uint8_t>(source)) - 128.0F) / 128.0F;
    case SPA_AUDIO_FORMAT_S16:
    case SPA_AUDIO_FORMAT_S16P:
        return static_cast<float>(loadUnaligned<std::int16_t>(source)) / 32768.0F;
    case SPA_AUDIO_FORMAT_S24_32:
    case SPA_AUDIO_FORMAT_S24_32P:
        return static_cast<float>(loadUnaligned<std::int32_t>(source)) / 8388608.0F;
    case SPA_AUDIO_FORMAT_S32:
    case SPA_AUDIO_FORMAT_S32P:
        return static_cast<float>(loadUnaligned<std::int32_t>(source)) / 2147483648.0F;
    case SPA_AUDIO_FORMAT_S24:
    case SPA_AUDIO_FORMAT_S24P: {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(source);
        std::int32_t value = static_cast<std::int32_t>(bytes[0]) |
                             (static_cast<std::int32_t>(bytes[1]) << 8) |
                             (static_cast<std::int32_t>(bytes[2]) << 16);
        if ((value & 0x00800000) != 0) {
            value |= static_cast<std::int32_t>(0xFF000000U);
        }
        return static_cast<float>(value) / 8388608.0F;
    }
    default:
        return 0.0F;
    }
}

[[nodiscard]] const std::byte* chunkStart(const spa_data& data) noexcept {
    if (data.data == nullptr || data.chunk == nullptr || data.maxsize == 0) {
        return nullptr;
    }
    const std::size_t offset = data.chunk->offset % data.maxsize;
    return static_cast<const std::byte*>(data.data) + offset;
}

[[nodiscard]] std::size_t contiguousChunkSize(const spa_data& data) noexcept {
    if (data.data == nullptr || data.chunk == nullptr || data.maxsize == 0) {
        return 0;
    }
    const std::size_t offset = data.chunk->offset % data.maxsize;
    return std::min<std::size_t>(data.chunk->size, data.maxsize - offset);
}

} // namespace

PipeWireCaptureStream::PipeWireCaptureStream(pw_thread_loop* const threadLoop,
                                             pw_core* const core,
                                             AudioRingBuffer& ringBuffer,
                                             const std::uint32_t targetNodeId,
                                             const spa_audio_format desiredFormat,
                                             const std::uint32_t desiredSampleRate,
                                             const std::uint32_t desiredChannels,
                                             StatusCallback statusCallback)
    : threadLoop_(threadLoop),
      core_(core),
      ringBuffer_(&ringBuffer),
      targetNodeId_(targetNodeId),
      desiredFormat_(desiredFormat),
      desiredSampleRate_(desiredSampleRate),
      desiredChannels_(desiredChannels),
      statusCallback_(std::move(statusCallback)) {}

PipeWireCaptureStream::~PipeWireCaptureStream() {
    destroy();
}

bool PipeWireCaptureStream::start() {
    if (threadLoop_ == nullptr || core_ == nullptr || ringBuffer_ == nullptr ||
        desiredFormat_ == SPA_AUDIO_FORMAT_UNKNOWN || desiredSampleRate_ == 0 ||
        desiredChannels_ == 0 || desiredChannels_ > SPA_AUDIO_MAX_CHANNELS ||
        bytesPerSample(desiredFormat_) == 0) {
        setStatus(StreamStatus::Error);
        return false;
    }

    const bool lockNeeded = !pw_thread_loop_in_thread(threadLoop_);
    if (lockNeeded) {
        pw_thread_loop_lock(threadLoop_);
    }

    if (stream_ != nullptr) {
        const pw_stream_state currentState = pw_stream_get_state(stream_, nullptr);
        if (currentState != PW_STREAM_STATE_UNCONNECTED && currentState != PW_STREAM_STATE_ERROR) {
            if (lockNeeded) {
                pw_thread_loop_unlock(threadLoop_);
            }
            return true;
        }
    }

    if (stream_ == nullptr) {
        pw_properties* properties = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Music",
            PW_KEY_APP_NAME, "Abracapture",
            nullptr);
        stream_ = pw_stream_new(core_, "Abracapture capture", properties);
        if (stream_ == nullptr) {
            if (lockNeeded) {
                pw_thread_loop_unlock(threadLoop_);
            }
            setStatus(StreamStatus::Error);
            return false;
        }

        static const pw_stream_events events = [] {
            pw_stream_events value{};
            value.version = PW_VERSION_STREAM_EVENTS;
            value.state_changed = [](void* data, pw_stream_state oldState,
                                     pw_stream_state state, const char* error) {
                onStateChanged(data, static_cast<int>(oldState), static_cast<int>(state), error);
            };
            value.param_changed = onParamChanged;
            value.process = on_process;
            return value;
        }();
        pw_stream_add_listener(stream_, &streamListener_, &events, this);
        streamListenerInstalled_ = true;
    }

    alignas(8) std::array<std::byte, 1024> podStorage{};
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(podStorage.data(), podStorage.size());
    spa_audio_info_raw requested{};
    requested.format = desiredFormat_;
    requested.rate = desiredSampleRate_;
    requested.channels = desiredChannels_;
    requested.flags = SPA_AUDIO_FLAG_UNPOSITIONED;
    const spa_pod* params[]{spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &requested)};

    negotiatedFormat_.store(SPA_AUDIO_FORMAT_UNKNOWN, std::memory_order_release);
    negotiatedSampleRate_.store(0, std::memory_order_release);
    negotiatedChannels_.store(0, std::memory_order_release);
    setStatus(StreamStatus::Connecting);
    const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                    PW_STREAM_FLAG_MAP_BUFFERS |
                                                    PW_STREAM_FLAG_RT_PROCESS);
    const int result = pw_stream_connect(stream_, PW_DIRECTION_INPUT, targetNodeId_, flags,
                                         params, 1);
    if (lockNeeded) {
        pw_thread_loop_unlock(threadLoop_);
    }
    if (result < 0) {
        setStatus(StreamStatus::Error);
        return false;
    }
    return true;
}

void PipeWireCaptureStream::stop() noexcept {
    if (threadLoop_ == nullptr || stream_ == nullptr) {
        setStatus(StreamStatus::Disconnected);
        return;
    }
    const bool lockNeeded = !pw_thread_loop_in_thread(threadLoop_);
    if (lockNeeded) {
        pw_thread_loop_lock(threadLoop_);
    }
    static_cast<void>(pw_stream_set_active(stream_, false));
    static_cast<void>(pw_stream_disconnect(stream_));
    if (lockNeeded) {
        pw_thread_loop_unlock(threadLoop_);
    }
    setStatus(StreamStatus::Disconnected);
}

void PipeWireCaptureStream::destroy() noexcept {
    if (threadLoop_ == nullptr || stream_ == nullptr) {
        return;
    }
    const bool lockNeeded = !pw_thread_loop_in_thread(threadLoop_);
    if (lockNeeded) {
        pw_thread_loop_lock(threadLoop_);
    }
    static_cast<void>(pw_stream_disconnect(stream_));
    if (streamListenerInstalled_) {
        spa_hook_remove(&streamListener_);
        streamListenerInstalled_ = false;
    }
    pw_stream_destroy(stream_);
    stream_ = nullptr;
    if (lockNeeded) {
        pw_thread_loop_unlock(threadLoop_);
    }
    setStatus(StreamStatus::Disconnected);
}

spa_audio_format PipeWireCaptureStream::negotiatedFormat() const noexcept {
    return negotiatedFormat_.load(std::memory_order_acquire);
}

std::uint32_t PipeWireCaptureStream::negotiatedSampleRate() const noexcept {
    return negotiatedSampleRate_.load(std::memory_order_acquire);
}

std::uint32_t PipeWireCaptureStream::negotiatedChannels() const noexcept {
    return negotiatedChannels_.load(std::memory_order_acquire);
}

bool PipeWireCaptureStream::isNativeCapture() const noexcept {
    return negotiatedFormat() == desiredFormat_ &&
           negotiatedSampleRate() == desiredSampleRate_ &&
           negotiatedChannels() == desiredChannels_;
}

PipeWireCaptureStream::StreamStatus PipeWireCaptureStream::status() const noexcept {
    return status_.load(std::memory_order_acquire);
}

bool PipeWireCaptureStream::visualizationSnapshot(
    CaptureVisualizationSnapshot& snapshot) const noexcept {
    for (int attempt = 0; attempt < 3; ++attempt) {
        const std::uint64_t sequence = visualizationSequence_.load(std::memory_order_acquire);
        if (sequence == 0) {
            return false;
        }
        const VisualizationSlot& slot = visualizationSlots_[sequence & 1U];
        const std::uint64_t versionBefore = slot.version.load(std::memory_order_acquire);
        if ((versionBefore & 1U) != 0) {
            continue;
        }
        const std::size_t count = std::min(slot.sampleCount.load(std::memory_order_relaxed),
                                           VisualizationSampleCapacity);
        for (std::size_t index = 0; index < count; ++index) {
            snapshot.samples[index] = slot.samples[index].load(std::memory_order_relaxed);
        }
        snapshot.sampleCount = count;
        snapshot.peak = slot.peak.load(std::memory_order_relaxed);
        snapshot.rms = slot.rms.load(std::memory_order_relaxed);
        snapshot.sequence = sequence;
        const std::uint64_t versionAfter = slot.version.load(std::memory_order_acquire);
        if (versionBefore == versionAfter && (versionAfter & 1U) == 0 &&
            visualizationSequence_.load(std::memory_order_acquire) == sequence) {
            return true;
        }
    }
    return false;
}

void PipeWireCaptureStream::on_process(void* const data) noexcept {
    static_cast<PipeWireCaptureStream*>(data)->process();
}

void PipeWireCaptureStream::onParamChanged(void* const data, const std::uint32_t id,
                                           const spa_pod* const param) noexcept {
    static_cast<PipeWireCaptureStream*>(data)->handleParamChanged(id, param);
}

void PipeWireCaptureStream::onStateChanged(void* const data, int, const int state,
                                           const char*) noexcept {
    static_cast<PipeWireCaptureStream*>(data)->handleStateChanged(state);
}

void PipeWireCaptureStream::process() noexcept {
    pw_buffer* const pipeWireBuffer = pw_stream_dequeue_buffer(stream_);
    if (pipeWireBuffer == nullptr) {
        return;
    }

    spa_buffer* const buffer = pipeWireBuffer->buffer;
    const spa_audio_format format = negotiatedFormat_.load(std::memory_order_relaxed);
    const std::size_t channels = negotiatedChannels_.load(std::memory_order_relaxed);
    const std::size_t sampleBytes = bytesPerSample(format);
    std::array<float, ConversionSampleCapacity> converted{};

    if (buffer != nullptr && channels != 0 && sampleBytes != 0) {
        if (!isPlanar(format) && buffer->n_datas >= 1) {
            const spa_data& data = buffer->datas[0];
            const std::byte* const source = chunkStart(data);
            const std::size_t frameStride = data.chunk != nullptr && data.chunk->stride > 0
                                                ? static_cast<std::size_t>(data.chunk->stride)
                                                : sampleBytes * channels;
            const std::size_t byteCount = contiguousChunkSize(data);
            if (source != nullptr && frameStride >= sampleBytes * channels) {
                const std::size_t frameCount = byteCount / frameStride;
                std::size_t frameOffset = 0;
                while (frameOffset < frameCount) {
                    const std::size_t framesThisPass = std::min(
                        frameCount - frameOffset, ConversionSampleCapacity / channels);
                    if (framesThisPass == 0) {
                        break;
                    }
                    const std::size_t samplesThisPass = framesThisPass * channels;
                    for (std::size_t frame = 0; frame < framesThisPass; ++frame) {
                        const std::byte* const frameData =
                            source + (frameOffset + frame) * frameStride;
                        for (std::size_t channel = 0; channel < channels; ++channel) {
                            converted[frame * channels + channel] =
                                pcmToFloat(frameData + channel * sampleBytes, format);
                        }
                    }
                    static_cast<void>(ringBuffer_->write(converted.data(), samplesThisPass));
                    publishVisualization(converted.data(), samplesThisPass);
                    frameOffset += framesThisPass;
                }
            }
        } else if (isPlanar(format) && buffer->n_datas >= channels) {
            std::size_t frameCount = std::numeric_limits<std::size_t>::max();
            for (std::size_t channel = 0; channel < channels; ++channel) {
                const spa_data& plane = buffer->datas[channel];
                const std::size_t stride = plane.chunk != nullptr && plane.chunk->stride > 0
                                               ? static_cast<std::size_t>(plane.chunk->stride)
                                               : sampleBytes;
                frameCount = std::min(frameCount, contiguousChunkSize(plane) / stride);
            }
            std::size_t frameOffset = 0;
            while (frameOffset < frameCount) {
                const std::size_t framesThisPass = std::min(
                    frameCount - frameOffset, ConversionSampleCapacity / channels);
                if (framesThisPass == 0) {
                    break;
                }
                const std::size_t samplesThisPass = framesThisPass * channels;
                for (std::size_t channel = 0; channel < channels; ++channel) {
                    const spa_data& plane = buffer->datas[channel];
                    const std::byte* const source = chunkStart(plane);
                    const std::size_t stride = plane.chunk != nullptr && plane.chunk->stride > 0
                                                   ? static_cast<std::size_t>(plane.chunk->stride)
                                                   : sampleBytes;
                    if (source == nullptr) {
                        continue;
                    }
                    for (std::size_t frame = 0; frame < framesThisPass; ++frame) {
                        converted[frame * channels + channel] =
                            pcmToFloat(source + (frameOffset + frame) * stride, format);
                    }
                }
                static_cast<void>(ringBuffer_->write(converted.data(), samplesThisPass));
                publishVisualization(converted.data(), samplesThisPass);
                frameOffset += framesThisPass;
            }
        }
    }
    static_cast<void>(pw_stream_queue_buffer(stream_, pipeWireBuffer));
}

void PipeWireCaptureStream::handleParamChanged(const std::uint32_t id,
                                               const spa_pod* const param) noexcept {
    if (id != SPA_PARAM_Format || param == nullptr) {
        return;
    }
    spa_audio_info_raw negotiated{};
    if (spa_format_audio_raw_parse(param, &negotiated) < 0 ||
        bytesPerSample(negotiated.format) == 0 || negotiated.rate == 0 ||
        negotiated.channels == 0 || negotiated.channels > SPA_AUDIO_MAX_CHANNELS) {
        setStatus(StreamStatus::Error);
        return;
    }
    negotiatedFormat_.store(negotiated.format, std::memory_order_release);
    negotiatedSampleRate_.store(negotiated.rate, std::memory_order_release);
    negotiatedChannels_.store(negotiated.channels, std::memory_order_release);
    setStatus(StreamStatus::FormatRenegotiated);
}

void PipeWireCaptureStream::handleStateChanged(const int state) noexcept {
    switch (static_cast<pw_stream_state>(state)) {
    case PW_STREAM_STATE_STREAMING:
    case PW_STREAM_STATE_PAUSED:
        setStatus(StreamStatus::Connected);
        break;
    case PW_STREAM_STATE_ERROR:
        setStatus(StreamStatus::Error);
        break;
    case PW_STREAM_STATE_UNCONNECTED:
        setStatus(StreamStatus::Disconnected);
        break;
    default:
        break;
    }
}

void PipeWireCaptureStream::setStatus(const StreamStatus newStatus) noexcept {
    if (status_.exchange(newStatus, std::memory_order_acq_rel) == newStatus) {
        return;
    }
    if (statusCallback_) {
        try {
            statusCallback_(newStatus);
        } catch (...) {
            // Exceptions must not cross the PipeWire C callback boundary.
        }
    }
}

void PipeWireCaptureStream::publishVisualization(const float* const samples,
                                                 const std::size_t sampleCount) noexcept {
    if (samples == nullptr || sampleCount == 0) {
        return;
    }
    const std::uint64_t sequence = visualizationSequence_.load(std::memory_order_relaxed) + 1;
    VisualizationSlot& slot = visualizationSlots_[sequence & 1U];
    slot.version.store(sequence * 2U + 1U, std::memory_order_release);

    const std::size_t step = std::max<std::size_t>(
        1, (sampleCount + VisualizationSampleCapacity - 1) / VisualizationSampleCapacity);
    std::size_t outputCount = 0;
    float peak = 0.0F;
    double squareSum = 0.0;
    for (std::size_t index = 0;
         index < sampleCount && outputCount < VisualizationSampleCapacity;
         index += step) {
        const float sample = samples[index];
        slot.samples[outputCount].store(sample, std::memory_order_relaxed);
        peak = std::max(peak, std::fabs(sample));
        squareSum += static_cast<double>(sample) * static_cast<double>(sample);
        ++outputCount;
    }
    slot.sampleCount.store(outputCount, std::memory_order_relaxed);
    slot.peak.store(peak, std::memory_order_relaxed);
    slot.rms.store(static_cast<float>(std::sqrt(squareSum / static_cast<double>(outputCount))),
                   std::memory_order_relaxed);
    slot.version.store(sequence * 2U + 2U, std::memory_order_release);
    visualizationSequence_.store(sequence, std::memory_order_release);
}

} // namespace abracapture::audio
