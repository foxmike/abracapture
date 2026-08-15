#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace abracapture::audio {

struct CaptureMetricsSnapshot {
    std::uint64_t framesCaptured = 0;
    float peakLevel = 0.0F;
    float rmsLevel = 0.0F;
    std::uint64_t overflowCount = 0;
    std::uint64_t underrunCount = 0;
    std::uint64_t xrunCount = 0;
    std::uint64_t timestampNs = 0;
};

static_assert(std::is_trivially_copyable_v<CaptureMetricsSnapshot>);
static_assert(sizeof(CaptureMetricsSnapshot) <= 64);

class CaptureMetrics {
public:
    void publish(const CaptureMetricsSnapshot& snapshot) noexcept;

    [[nodiscard]] CaptureMetricsSnapshot snapshot() const noexcept;

private:
    struct Slot {
        std::atomic<std::uint64_t> framesCaptured{0};
        std::atomic<float> peakLevel{0.0F};
        std::atomic<float> rmsLevel{0.0F};
        std::atomic<std::uint64_t> overflowCount{0};
        std::atomic<std::uint64_t> underrunCount{0};
        std::atomic<std::uint64_t> xrunCount{0};
        std::atomic<std::uint64_t> timestampNs{0};
    };

    Slot slots_[2];
    std::atomic<std::uint64_t> sequence_{0};
};

static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

} // namespace abracapture::audio
