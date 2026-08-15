#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace abracapture::audio {

// Audio samples must remain trivially copyable so producer and consumer can
// copy them without invoking user code in either realtime path.
static_assert(std::is_trivially_copyable_v<float>);

class AudioRingBuffer {
public:
    explicit AudioRingBuffer(std::size_t capacityFrames);

    // Returns the number of input frames that could not be queued.
    [[nodiscard]] std::size_t write(const float* frames, std::size_t frameCount) noexcept;

    // Returns the number of frames copied. A result below frameCount is an
    // underrun; the missing-frame count is accumulated by underrunCount().
    [[nodiscard]] std::size_t read(float* frames, std::size_t frameCount) noexcept;

    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::size_t available() const noexcept;
    [[nodiscard]] std::uint64_t overflowCount() const noexcept;
    [[nodiscard]] std::uint64_t underrunCount() const noexcept;

    // Call only while neither producer nor consumer is accessing the buffer.
    void reset() noexcept;

private:
    std::vector<float> frames_;
    std::size_t mask_;
    std::atomic<std::uint64_t> writePosition_{0};
    std::atomic<std::uint64_t> readPosition_{0};
    std::atomic<std::uint64_t> overflowFrames_{0};
    std::atomic<std::uint64_t> underrunFrames_{0};
};

} // namespace abracapture::audio
