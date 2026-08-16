#include "RecordingSink.hpp"

#include "FlacWriter.hpp"
#include "OutputPathPolicy.hpp"
#include "WavWriter.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <limits>
#include <system_error>

namespace abracapture::recording {

bool RecordingSink::open(const AudioFormat& format,
                         const CaptureMetadata& captureMetadata,
                         const OutputPathPolicy& outputPathPolicy) {
    return open(FormatConversion{format, format, ConversionProvenance::Native},
                captureMetadata, outputPathPolicy);
}

bool RecordingSink::open(const FormatConversion& format,
                         const CaptureMetadata& captureMetadata,
                         const OutputPathPolicy& outputPathPolicy) {
    if (active_) {
        errorState_ = true;
        lastError_ = "Recording sink is already open";
        return false;
    }

    writer_.emplace<std::monostate>();
    outputPathPolicy_ = &outputPathPolicy;
    finalPath_.clear();
    temporaryPath_.clear();
    totalFrames_ = 0;
    sampleFormat_ = format.target.sampleFormat;
    sampleRate_ = format.target.sampleRate;
    errorState_ = false;
    lastError_.clear();

    if (format.source.sampleRate != format.target.sampleRate ||
        format.provenance == ConversionProvenance::Resampled) {
        provenance_ = ConversionProvenance::Resampled;
    } else if (((format.source.sampleFormat == SampleFormat::Float32 ||
                 format.source.sampleFormat == SampleFormat::Float64) &&
                (format.target.sampleFormat == SampleFormat::Int16 ||
                 format.target.sampleFormat == SampleFormat::Int24 ||
                 format.target.sampleFormat == SampleFormat::Int32)) ||
               format.provenance == ConversionProvenance::Quantized) {
        provenance_ = ConversionProvenance::Quantized;
    } else if (format.source == format.target &&
               format.provenance == ConversionProvenance::Native) {
        provenance_ = ConversionProvenance::Native;
    } else {
        // In particular, widening an integer or floating-point representation
        // is a conversion, not evidence that the source had higher resolution.
        provenance_ = ConversionProvenance::Converted;
    }

    try {
        CaptureMetadata pathMetadata = captureMetadata;
        pathMetadata.sampleRate = format.target.sampleRate;
        pathMetadata.channels = format.target.channels;
        finalPath_ = outputPathPolicy.resolveOutputPath(pathMetadata);
    } catch (const std::exception& exception) {
        errorState_ = true;
        lastError_ = exception.what();
        return false;
    }

    temporaryPath_ = finalPath_.parent_path() /
                     ("." + finalPath_.filename().string() + ".tmp");
    std::error_code pathError;
    if (std::filesystem::exists(temporaryPath_, pathError)) {
        const auto baseTemporaryPath = temporaryPath_;
        bool available = false;
        for (std::uint32_t suffix = 1; suffix <= 9999; ++suffix) {
            temporaryPath_ = baseTemporaryPath.string() + "." + std::to_string(suffix);
            pathError.clear();
            if (!std::filesystem::exists(temporaryPath_, pathError) && !pathError) {
                available = true;
                break;
            }
        }
        if (!available) {
            errorState_ = true;
            lastError_ = "Unable to allocate a unique temporary recording path";
            return false;
        }
    } else if (pathError) {
        errorState_ = true;
        lastError_ = "Unable to inspect temporary recording path: " + pathError.message();
        return false;
    }

    std::string container = finalPath_.extension().string();
    if (!container.empty() && container.front() == '.') {
        container.erase(container.begin());
    }
    std::transform(container.begin(), container.end(), container.begin(),
                   [](const unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });

    bool opened = false;
    if (container == "wav" || container == "wave" || container == "rf64") {
        auto& writer = writer_.emplace<WavWriter>();
        opened = writer.open(temporaryPath_, format.target.channels,
                             format.target.sampleRate, format.target.sampleFormat);
        if (!opened || !writer.lastError().empty()) {
            lastError_ = writer.lastError();
        }
    } else if (container == "flac") {
        auto& writer = writer_.emplace<FlacWriter>();
        opened = writer.open(temporaryPath_, format.target.channels,
                             format.target.sampleRate, format.target.sampleFormat);
        if (!opened || !writer.lastError().empty()) {
            lastError_ = writer.lastError();
        }
    } else {
        lastError_ = "Unsupported recording container format: " + container;
    }

    if (!opened) {
        errorState_ = true;
        if (lastError_.empty()) {
            lastError_ = "Unable to open recording writer";
        }
        writer_.emplace<std::monostate>();
        std::error_code ignored;
        std::filesystem::remove(temporaryPath_, ignored);
        return false;
    }

    active_ = true;
    return true;
}

bool RecordingSink::pushSamples(const float* const buffer,
                                const std::size_t frameCount) {
    if (!active_ || errorState_) {
        if (!active_ && lastError_.empty()) {
            lastError_ = "Recording sink is not open";
        }
        return false;
    }

    auto* const writer = std::get_if<WavWriter>(&writer_);
    if (writer == nullptr) {
        errorState_ = true;
        lastError_ = "FLAC recording requires pre-quantized int32 sample input";
        return false;
    }
    if (frameCount > std::numeric_limits<std::uint64_t>::max() - totalFrames_) {
        errorState_ = true;
        lastError_ = "Total recording frame count overflow";
        return false;
    }

    const bool written = writer->write(buffer, frameCount);
    if (!written || !writer->lastError().empty()) {
        errorState_ = true;
        lastError_ = writer->lastError().empty() ? "Unable to write WAV samples"
                                                 : writer->lastError();
        return false;
    }
    totalFrames_ += frameCount;
    return true;
}

bool RecordingSink::pushSamples(const std::int32_t* const buffer,
                                const std::size_t frameCount) {
    if (!active_ || errorState_) {
        if (!active_ && lastError_.empty()) {
            lastError_ = "Recording sink is not open";
        }
        return false;
    }

    auto* const writer = std::get_if<FlacWriter>(&writer_);
    if (writer == nullptr) {
        errorState_ = true;
        lastError_ = "WAV recording requires floating-point sample input";
        return false;
    }
    if (frameCount > std::numeric_limits<std::uint64_t>::max() - totalFrames_) {
        errorState_ = true;
        lastError_ = "Total recording frame count overflow";
        return false;
    }

    const bool written = writer->write(buffer, frameCount);
    if (!written || !writer->lastError().empty()) {
        errorState_ = true;
        lastError_ = writer->lastError().empty() ? "Unable to write FLAC samples"
                                                 : writer->lastError();
        return false;
    }
    totalFrames_ += frameCount;
    return true;
}

CaptureResult RecordingSink::finalize() {
    CaptureResult result;
    result.finalPath = finalPath_;
    result.totalFrames = totalFrames_;
    result.duration = sampleRate_ == 0
                          ? std::chrono::duration<double>::zero()
                          : std::chrono::duration<double>(
                                static_cast<double>(totalFrames_) /
                                static_cast<double>(sampleRate_));
    result.provenance = provenance_;
    result.sampleFormat = sampleFormat_;
    result.sampleRate = sampleRate_;

    if (!active_) {
        result.error = lastError_.empty() ? "Recording sink is not open" : lastError_;
        return result;
    }

    bool writerFinalized = false;
    std::string writerError;
    if (auto* const writer = std::get_if<WavWriter>(&writer_)) {
        writerFinalized = writer->finalize();
        writerError = writer->lastError();
        if (!writerFinalized && writer->isOpen()) {
            writer->abort();
        }
    } else if (auto* const writer = std::get_if<FlacWriter>(&writer_)) {
        writerFinalized = writer->finalize();
        writerError = writer->lastError();
        if (!writerFinalized && writer->isOpen()) {
            writer->abort();
        }
    }

    active_ = false;
    writer_.emplace<std::monostate>();

    if (errorState_) {
        result.error = lastError_;
        return result;
    }
    if (!writerFinalized || !writerError.empty()) {
        errorState_ = true;
        lastError_ = writerError.empty() ? "Unable to finalize recording writer"
                                         : writerError;
        result.error = lastError_;
        return result;
    }

    std::error_code renameError;
    std::filesystem::rename(temporaryPath_, finalPath_, renameError);
    if (renameError) {
        errorState_ = true;
        lastError_ = "Unable to atomically finalize recording: " + renameError.message();
        result.error = lastError_;
        // The writer is closed and the temporary file is deliberately retained
        // so the capture can be recovered after a rename failure.
        return result;
    }

    result.success = true;
    lastError_.clear();
    temporaryPath_.clear();
    return result;
}

void RecordingSink::abort() {
    if (auto* const writer = std::get_if<WavWriter>(&writer_)) {
        writer->abort();
    } else if (auto* const writer = std::get_if<FlacWriter>(&writer_)) {
        writer->abort();
    }

    writer_.emplace<std::monostate>();
    if (!temporaryPath_.empty()) {
        std::error_code removeError;
        std::filesystem::remove(temporaryPath_, removeError);
        if (removeError) {
            lastError_ = "Unable to remove temporary recording: " + removeError.message();
        }
    }

    active_ = false;
    errorState_ = false;
    temporaryPath_.clear();
}

} // namespace abracapture::recording
