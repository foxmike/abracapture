#pragma once

#include "../core/AudioFormat.hpp"

#include <QJsonObject>

#include <cstdint>
#include <filesystem>
#include <string>

namespace abracapture::recording {

enum class CaptureKind {
    Native,
    Quantized,
    Resampled,
    Converted,
};

// Per-capture provenance supplied by the recording pipeline. This value type
// only describes a completed capture; it does not perform file operations.
class CaptureIntegrity {
public:
    std::string sourceStream;
    SampleFormat negotiatedFormat = SampleFormat::Int16;
    std::uint32_t negotiatedRate = 0;
    SampleFormat fileFormat = SampleFormat::Int16;
    std::uint32_t fileRate = 0;
    std::uint64_t xrunCount = 0;
    std::uint64_t totalFrames = 0;
    double durationSeconds = 0.0;
    std::filesystem::path filePath;

    void setNegotiatedFormat(SampleFormat format, std::uint32_t rate) noexcept;
    void setFileFormat(SampleFormat format, std::uint32_t rate) noexcept;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] std::string summary() const;
};

} // namespace abracapture::recording
