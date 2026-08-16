#pragma once

#include "../core/AudioFormat.hpp"
#include "FlacWriter.hpp"
#include "OutputPathPolicy.hpp"
#include "WavWriter.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

namespace abracapture::recording {

struct CaptureResult {
    std::filesystem::path finalPath;
    std::uint64_t totalFrames = 0;
    std::chrono::duration<double> duration{};
    ConversionProvenance provenance = ConversionProvenance::Native;
    SampleFormat sampleFormat = SampleFormat::Int16;
    std::uint32_t sampleRate = 0;
    bool success = false;
    std::string error;
};

// Owns the file writer for one capture session. All methods must be called
// serially from the dedicated recording writer thread, never from a PipeWire
// realtime callback. RecordingSink provides no internal synchronization.
class RecordingSink {
public:
    RecordingSink() = default;

    RecordingSink(const RecordingSink&) = delete;
    RecordingSink& operator=(const RecordingSink&) = delete;

    [[nodiscard]] bool open(const FormatConversion& format,
                            const CaptureMetadata& captureMetadata,
                            const OutputPathPolicy& outputPathPolicy);
    [[nodiscard]] bool open(const AudioFormat& format,
                            const CaptureMetadata& captureMetadata,
                            const OutputPathPolicy& outputPathPolicy);

    [[nodiscard]] bool pushSamples(const float* buffer, std::size_t frameCount);
    [[nodiscard]] bool pushSamples(const std::int32_t* buffer, std::size_t frameCount);

    [[nodiscard]] CaptureResult finalize();
    [[nodiscard]] const std::string& lastError() const noexcept;
    void abort();

private:
    std::variant<std::monostate, WavWriter, FlacWriter> writer_;
    const OutputPathPolicy* outputPathPolicy_ = nullptr;
    std::filesystem::path finalPath_;
    std::filesystem::path temporaryPath_;
    std::uint64_t totalFrames_ = 0;
    ConversionProvenance provenance_ = ConversionProvenance::Native;
    SampleFormat sampleFormat_ = SampleFormat::Int16;
    std::uint32_t sampleRate_ = 0;
    bool active_ = false;
    bool errorState_ = false;
    std::string lastError_;
};

} // namespace abracapture::recording
