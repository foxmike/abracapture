#pragma once

#include "../core/AudioFormat.hpp"

#include <sndfile.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace abracapture::recording {

// This class performs file I/O and must be used only from the non-realtime
// recording writer thread, never from a PipeWire realtime callback.
class WavWriter {
public:
    WavWriter() = default;
    ~WavWriter();

    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    [[nodiscard]] bool open(const std::filesystem::path& path,
                            std::uint16_t channels,
                            std::uint32_t sampleRate,
                            SampleFormat sampleFormat);
    [[nodiscard]] bool write(const float* interleavedFrames, std::size_t frameCount);
    [[nodiscard]] bool finalize();
    void abort();

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] ConversionProvenance provenance() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;

private:
    [[nodiscard]] bool setErrorFromSndfile(const char* operation);

    SNDFILE* handle_ = nullptr;
    std::filesystem::path path_;
    std::uint16_t channels_ = 0;
    SampleFormat sampleFormat_ = SampleFormat::Int16;
    ConversionProvenance provenance_ = ConversionProvenance::Native;
    std::uint64_t audioBytesWritten_ = 0;
    std::string lastError_;
};

} // namespace abracapture::recording
