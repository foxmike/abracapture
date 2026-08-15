#include "AudioFormat.hpp"

int bytesPerSample(SampleFormat format) {
    switch (format) {
    case SampleFormat::Int16:
        return 2;
    case SampleFormat::Int24:
        return 3;
    case SampleFormat::Int32:
    case SampleFormat::Float32:
        return 4;
    case SampleFormat::Float64:
        return 8;
    }

    return 0;
}

int bytesPerFrame(const AudioFormat& format) {
    return bytesPerSample(format.sampleFormat) * static_cast<int>(format.channels);
}

std::string_view sampleFormatName(SampleFormat format) {
    switch (format) {
    case SampleFormat::Int16:
        return "Int16";
    case SampleFormat::Int24:
        return "Int24";
    case SampleFormat::Int32:
        return "Int32";
    case SampleFormat::Float32:
        return "Float32";
    case SampleFormat::Float64:
        return "Float64";
    }

    return {};
}

std::string_view provenanceName(ConversionProvenance provenance) {
    switch (provenance) {
    case ConversionProvenance::Native:
        return "Native";
    case ConversionProvenance::Quantized:
        return "Quantized";
    case ConversionProvenance::Resampled:
        return "Resampled";
    case ConversionProvenance::Converted:
        return "Converted";
    }

    return {};
}
