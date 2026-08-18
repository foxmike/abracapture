#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace abracapture::recording {

struct CaptureMetadata {
    std::string source;
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    std::string format;
    std::uint64_t counter = 0;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    // Snapshot of the product-relevant PipeWire source metadata at capture start.
    std::string mediaTitle;
    std::string mediaName;
    std::string mediaAlbum;
    std::string applicationName;
    bool automatedCapture = false;
    bool useMediaMetadataNames = false;
    bool createDirectories = true;
};

class OutputPathPolicy {
public:
    OutputPathPolicy() = default;
    explicit OutputPathPolicy(std::filesystem::path baseDirectory,
                              std::string filenameTemplate = "{source}_{date}_{time}",
                              std::string defaultFormat = "wav");

    [[nodiscard]] const std::filesystem::path& baseDirectory() const noexcept;
    void setBaseDirectory(std::filesystem::path baseDirectory);

    [[nodiscard]] const std::string& filenameTemplate() const noexcept;
    void setFilenameTemplate(std::string filenameTemplate);

    [[nodiscard]] const std::string& defaultFormat() const noexcept;
    void setDefaultFormat(std::string defaultFormat);

    [[nodiscard]] std::filesystem::path resolveOutputPath(const CaptureMetadata& metadata) const;
    [[nodiscard]] bool validate() const;

    // Metadata is external input. This produces one safe filename/path component
    // while preserving ordinary UTF-8 text and punctuation.
    [[nodiscard]] static std::string sanitizePathComponent(std::string_view component);

private:
    std::filesystem::path baseDirectory_;
    std::string filenameTemplate_ = "{source}_{date}_{time}";
    std::string defaultFormat_ = "wav";
};

} // namespace abracapture::recording
