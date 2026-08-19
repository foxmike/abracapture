#pragma once

#include "../export/AudioDomainTypes.hpp"

#include <cstdint>
#include <string_view>

enum class SampleFormat : std::uint8_t {
    Int16,
    Int24,
    Int32,
    Float32,
    Float64,
};

// AudioFormat carries the speaker channel layout alongside rate / channels /
// sample format. The layout defaults to `unknown(channelCount)` only when the
// producer explicitly reports known positions; otherwise the producers leave
// it as the default `unknown(0)` and downstream code must not infer a layout
// from the channel count alone.
struct AudioFormat {
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    SampleFormat sampleFormat = SampleFormat::Int16;
    abracapture::exporting::AudioChannelLayout channelLayout;

    friend constexpr bool operator==(const AudioFormat& left, const AudioFormat& right) {
        return left.sampleRate == right.sampleRate &&
               left.channels == right.channels &&
               left.sampleFormat == right.sampleFormat &&
               left.channelLayout == right.channelLayout;
    }

    friend constexpr bool operator!=(const AudioFormat& left, const AudioFormat& right) {
        return !(left == right);
    }
};

enum class ConversionProvenance : std::uint8_t {
    Native,
    Quantized,
    Resampled,
    Converted,
};

struct FormatConversion {
    AudioFormat source;
    AudioFormat target;
    ConversionProvenance provenance = ConversionProvenance::Native;

    // Convenience: returns the source layout (or unknown if absent) for
    // downstream format negotiation. Lifetime is by-value and lock-free.
    [[nodiscard]] abracapture::exporting::AudioChannelLayout sourceLayout() const noexcept {
        return source.channelLayout;
    }
    [[nodiscard]] abracapture::exporting::AudioChannelLayout targetLayout() const noexcept {
        return target.channelLayout;
    }
};

int bytesPerSample(SampleFormat format);
int bytesPerFrame(const AudioFormat& format);
std::string_view sampleFormatName(SampleFormat format);
std::string_view provenanceName(ConversionProvenance provenance);
