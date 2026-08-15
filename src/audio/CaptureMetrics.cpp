#include "audio/CaptureMetrics.hpp"

namespace abracapture::audio {

void CaptureMetrics::publish(const CaptureMetricsSnapshot& value) noexcept {
    const std::uint64_t generation = sequence_.load(std::memory_order_relaxed) / 2U + 1U;
    const std::uint64_t writingSequence = generation * 2U + 1U;
    Slot& slot = slots_[generation & 1U];

    sequence_.store(writingSequence, std::memory_order_release);
    slot.framesCaptured.store(value.framesCaptured, std::memory_order_relaxed);
    slot.peakLevel.store(value.peakLevel, std::memory_order_relaxed);
    slot.rmsLevel.store(value.rmsLevel, std::memory_order_relaxed);
    slot.overflowCount.store(value.overflowCount, std::memory_order_relaxed);
    slot.underrunCount.store(value.underrunCount, std::memory_order_relaxed);
    slot.xrunCount.store(value.xrunCount, std::memory_order_relaxed);
    slot.timestampNs.store(value.timestampNs, std::memory_order_relaxed);
    sequence_.store(writingSequence + 1U, std::memory_order_release);
}

CaptureMetricsSnapshot CaptureMetrics::snapshot() const noexcept {
    CaptureMetricsSnapshot value{};
    for (;;) {
        const std::uint64_t firstSequence = sequence_.load(std::memory_order_acquire);
        if ((firstSequence & 1U) != 0U) {
            continue;
        }

        const Slot& slot = slots_[(firstSequence / 2U) & 1U];
        value.framesCaptured = slot.framesCaptured.load(std::memory_order_relaxed);
        value.peakLevel = slot.peakLevel.load(std::memory_order_relaxed);
        value.rmsLevel = slot.rmsLevel.load(std::memory_order_relaxed);
        value.overflowCount = slot.overflowCount.load(std::memory_order_relaxed);
        value.underrunCount = slot.underrunCount.load(std::memory_order_relaxed);
        value.xrunCount = slot.xrunCount.load(std::memory_order_relaxed);
        value.timestampNs = slot.timestampNs.load(std::memory_order_relaxed);

        if (sequence_.load(std::memory_order_acquire) == firstSequence) {
            return value;
        }
    }
}

} // namespace abracapture::audio
