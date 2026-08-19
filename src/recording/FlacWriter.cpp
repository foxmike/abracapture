#include "recording/FlacWriter.hpp"

#include <FLAC/stream_encoder.h>

#include <limits>
#include <string>
#include <system_error>

namespace abracapture::recording {
namespace {

constexpr unsigned kCompressionLevel = 5;

std::string encoderError(const char* const operation, const FLAC__StreamEncoder* const encoder) {
    std::string error(operation);
    if (encoder != nullptr) {
        // Query the state explicitly at the failure site; the resolved string
        // adds the underlying I/O detail for client-visible diagnostics.
        const auto state = FLAC__stream_encoder_get_state(encoder);
        const char* const detail = FLAC__stream_encoder_get_resolved_state_string(encoder);
        if (detail != nullptr && detail[0] != '\0') {
            error += ": ";
            error += detail;
        } else {
            error += ": ";
            error += FLAC__StreamEncoderStateString[state];
        }
    }
    return error;
}

} // namespace

FlacWriter::~FlacWriter() {
    abort();
}

bool FlacWriter::open(const std::filesystem::path& path,
                      const std::uint16_t channels,
                      const std::uint32_t sampleRate,
                      const SampleFormat sampleFormat) {
    return open(path, channels, sampleRate, sampleFormat,
                abracapture::exporting::AudioChannelLayout{});
}

bool FlacWriter::open(const std::filesystem::path& path,
                      const std::uint16_t channels,
                      const std::uint32_t sampleRate,
                      const SampleFormat sampleFormat,
                      const abracapture::exporting::AudioChannelLayout& layout) {
    switch (sampleFormat) {
    case SampleFormat::Int16:
        return open(path, channels, sampleRate, 16, layout);
    case SampleFormat::Int24:
        return open(path, channels, sampleRate, 24, layout);
    case SampleFormat::Int32:
        return open(path, channels, sampleRate, 32, layout);
    case SampleFormat::Float32:
    case SampleFormat::Float64:
        lastError_ = "FLAC supports integer samples only; floating-point sample formats are not supported";
        return false;
    }

    lastError_ = "Unsupported FLAC sample format";
    return false;
}

bool FlacWriter::open(const std::filesystem::path& path,
                      const std::uint16_t channels,
                      const std::uint32_t sampleRate,
                      const std::uint32_t bitsPerSample) {
    return open(path, channels, sampleRate, bitsPerSample,
                abracapture::exporting::AudioChannelLayout{});
}

bool FlacWriter::open(const std::filesystem::path& path,
                      const std::uint16_t channels,
                      const std::uint32_t sampleRate,
                      const std::uint32_t bitsPerSample,
                      const abracapture::exporting::AudioChannelLayout& layout) {
    if (isOpen()) {
        lastError_ = "FLAC writer is already open";
        return false;
    }
    if (path.empty() || channels == 0 || sampleRate == 0) {
        lastError_ = "FLAC writer requires a path, channels, and sample rate";
        return false;
    }
    if (bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
        lastError_ = "FLAC bits per sample must be 16, 24, or 32";
        return false;
    }

    encoder_ = FLAC__stream_encoder_new();
    if (encoder_ == nullptr) {
        lastError_ = "Unable to allocate FLAC encoder";
        return false;
    }

    const bool configured =
        FLAC__stream_encoder_set_channels(encoder_, channels) &&
        FLAC__stream_encoder_set_sample_rate(encoder_, sampleRate) &&
        FLAC__stream_encoder_set_bits_per_sample(encoder_, bitsPerSample) &&
        FLAC__stream_encoder_set_compression_level(encoder_, kCompressionLevel);
    if (!configured) {
        lastError_ = encoderError("Unable to configure FLAC encoder", encoder_);
        FLAC__stream_encoder_delete(encoder_);
        encoder_ = nullptr;
        return false;
    }

    // FLAC's ordinary multichannel stream stores ordered sample channels, but
    // it has no universal speaker-position map. Keep the negotiated layout in
    // the capture/index metadata; do not invent a WAVEFORMATEXTENSIBLE-style
    // Vorbis comment and present it as a portable FLAC layout.
    static_cast<void>(layout);

    const auto pathString = path.string();
    const auto initStatus = FLAC__stream_encoder_init_file(encoder_, pathString.c_str(), nullptr, nullptr);
    if (initStatus != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        lastError_ = "Unable to open FLAC file: ";
        lastError_ += FLAC__StreamEncoderInitStatusString[initStatus];
        FLAC__stream_encoder_delete(encoder_);
        encoder_ = nullptr;
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return false;
    }

    path_ = path;
    channels_ = channels;
    bitsPerSample_ = bitsPerSample;
    totalFramesWritten_ = 0;
    provenance_ = ConversionProvenance::Native;
    lastError_.clear();
    return true;
}

bool FlacWriter::write(const std::int32_t* const interleavedFrames, const std::size_t frameCount) {
    if (!isOpen()) {
        lastError_ = "FLAC writer is not open";
        return false;
    }
    if (frameCount == 0) {
        return true;
    }
    if (interleavedFrames == nullptr) {
        lastError_ = "Cannot write null audio frames";
        return false;
    }
    if (frameCount > std::numeric_limits<std::uint32_t>::max()) {
        lastError_ = "Audio frame count exceeds libFLAC limits";
        return false;
    }
    if (totalFramesWritten_ > std::numeric_limits<std::uint64_t>::max() - frameCount) {
        lastError_ = "Total FLAC frame count overflow";
        return false;
    }

    if (!FLAC__stream_encoder_process_interleaved(
            encoder_, interleavedFrames, static_cast<std::uint32_t>(frameCount))) {
        lastError_ = encoderError("Unable to encode FLAC frames", encoder_);
        return false;
    }

    totalFramesWritten_ += frameCount;
    return true;
}

bool FlacWriter::finalize() {
    if (!isOpen()) {
        lastError_ = "FLAC writer is not open";
        return false;
    }

    if (!FLAC__stream_encoder_finish(encoder_)) {
        lastError_ = encoderError("Unable to finalize FLAC file", encoder_);
        FLAC__stream_encoder_delete(encoder_);
        encoder_ = nullptr;
        channels_ = 0;
        bitsPerSample_ = 0;
        totalFramesWritten_ = 0;
        return false;
    }

    FLAC__stream_encoder_delete(encoder_);
    encoder_ = nullptr;
    path_.clear();
    channels_ = 0;
    bitsPerSample_ = 0;
    totalFramesWritten_ = 0;
    return true;
}

void FlacWriter::abort() {
    if (encoder_ != nullptr) {
        FLAC__stream_encoder_finish(encoder_);
        FLAC__stream_encoder_delete(encoder_);
        encoder_ = nullptr;
    }
    if (!path_.empty()) {
        std::error_code error;
        std::filesystem::remove(path_, error);
        if (error) {
            lastError_ = "Unable to remove partial FLAC file: " + error.message();
        }
    }
    path_.clear();
    channels_ = 0;
    bitsPerSample_ = 0;
    totalFramesWritten_ = 0;
}

bool FlacWriter::isOpen() const noexcept {
    return encoder_ != nullptr;
}

ConversionProvenance FlacWriter::provenance() const noexcept {
    return provenance_;
}

const std::string& FlacWriter::lastError() const noexcept {
    return lastError_;
}

} // namespace abracapture::recording
