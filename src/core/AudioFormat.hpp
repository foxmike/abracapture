#pragma once

#include <cstdint>
#include <string_view>

enum class SampleFormat : std::uint8_t {
    Int16,
    Int24,
    Int32,
    Float32,
    Float64,
};

struct AudioFormat {
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    SampleFormat sampleFormat = SampleFormat::Int16;

    friend constexpr bool operator==(const AudioFormat& left, const AudioFormat& right) {
        return left.sampleRate == right.sampleRate &&
               left.channels == right.channels &&
               left.sampleFormat == right.sampleFormat;
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
};

int bytesPerSample(SampleFormat format);
int bytesPerFrame(const AudioFormat& format);
std::string_view sampleFormatName(SampleFormat format);
std::string_view provenanceName(ConversionProvenance provenance);
