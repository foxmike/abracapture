#pragma once

#include "../core/AudioFormat.hpp"

#include <FLAC/stream_encoder.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace abracapture::recording {

// Lossless FLAC writer for integer PCM. FLAC does not support floating-point
// samples, so Float32 and Float64 formats are rejected rather than converted.
// Input samples are interleaved signed values, right-justified in int32_t, and
// must already be in the range of the selected 16-, 24-, or 32-bit depth.
//
// This class performs file I/O and must be used only from the non-realtime
// recording writer thread, never from a PipeWire realtime callback.
class FlacWriter {
public:
    FlacWriter() = default;
    ~FlacWriter();

    FlacWriter(const FlacWriter&) = delete;
    FlacWriter& operator=(const FlacWriter&) = delete;

    [[nodiscard]] bool open(const std::filesystem::path& path,
                            std::uint16_t channels,
                            std::uint32_t sampleRate,
                            std::uint32_t bitsPerSample);
    [[nodiscard]] bool open(const std::filesystem::path& path,
                            std::uint16_t channels,
                            std::uint32_t sampleRate,
                            SampleFormat sampleFormat);
    [[nodiscard]] bool write(const std::int32_t* interleavedFrames, std::size_t frameCount);
    [[nodiscard]] bool finalize();
    void abort();

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] ConversionProvenance provenance() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;

private:
    FLAC__StreamEncoder* encoder_ = nullptr;
    std::filesystem::path path_;
    std::uint16_t channels_ = 0;
    std::uint32_t bitsPerSample_ = 0;
    std::uint64_t totalFramesWritten_ = 0;
    ConversionProvenance provenance_ = ConversionProvenance::Native;
    std::string lastError_;
};

} // namespace abracapture::recording
