#include "audio/AudioRingBuffer.hpp"

#include <algorithm>
#include <stdexcept>

namespace abracapture::audio {

AudioRingBuffer::AudioRingBuffer(const std::size_t capacityFrames)
    : frames_(capacityFrames),
      mask_(capacityFrames - 1) {
    if (capacityFrames == 0 || (capacityFrames & (capacityFrames - 1)) != 0) {
        throw std::invalid_argument("AudioRingBuffer capacity must be a non-zero power of two");
    }
}

std::size_t AudioRingBuffer::write(const float* const frames, const std::size_t frameCount) noexcept {
    const auto writePosition = writePosition_.load(std::memory_order_relaxed);
    const auto readPosition = readPosition_.load(std::memory_order_acquire);
    const auto usedFrames = writePosition - readPosition;
    const auto writableFrames = capacity() - static_cast<std::size_t>(usedFrames);
    const auto framesToWrite = std::min(frameCount, writableFrames);

    for (std::size_t index = 0; index < framesToWrite; ++index) {
        frames_[(writePosition + index) & mask_] = frames[index];
    }

    writePosition_.store(writePosition + framesToWrite, std::memory_order_release);

    const auto droppedFrames = frameCount - framesToWrite;
    if (droppedFrames != 0) {
        overflowFrames_.fetch_add(droppedFrames, std::memory_order_relaxed);
    }
    return droppedFrames;
}

std::size_t AudioRingBuffer::read(float* const frames, const std::size_t frameCount) noexcept {
    const auto readPosition = readPosition_.load(std::memory_order_relaxed);
    const auto writePosition = writePosition_.load(std::memory_order_acquire);
    const auto availableFrames = static_cast<std::size_t>(writePosition - readPosition);
    const auto framesToRead = std::min(frameCount, availableFrames);

    for (std::size_t index = 0; index < framesToRead; ++index) {
        frames[index] = frames_[(readPosition + index) & mask_];
    }

    readPosition_.store(readPosition + framesToRead, std::memory_order_release);

    const auto missingFrames = frameCount - framesToRead;
    if (missingFrames != 0) {
        underrunFrames_.fetch_add(missingFrames, std::memory_order_relaxed);
    }
    return framesToRead;
}

std::size_t AudioRingBuffer::capacity() const noexcept {
    return frames_.size();
}

std::size_t AudioRingBuffer::available() const noexcept {
    const auto writePosition = writePosition_.load(std::memory_order_acquire);
    const auto readPosition = readPosition_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(writePosition - readPosition);
}

std::uint64_t AudioRingBuffer::overflowCount() const noexcept {
    return overflowFrames_.load(std::memory_order_relaxed);
}

std::uint64_t AudioRingBuffer::underrunCount() const noexcept {
    return underrunFrames_.load(std::memory_order_relaxed);
}

void AudioRingBuffer::reset() noexcept {
    writePosition_.store(0, std::memory_order_relaxed);
    readPosition_.store(0, std::memory_order_relaxed);
    overflowFrames_.store(0, std::memory_order_relaxed);
    underrunFrames_.store(0, std::memory_order_relaxed);
}

} // namespace abracapture::audio
