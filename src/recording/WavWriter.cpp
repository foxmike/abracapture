#include "recording/WavWriter.hpp"

#include <sndfile.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace abracapture::recording {
namespace {

int sndfileEncoding(const SampleFormat sampleFormat) {
    switch (sampleFormat) {
    case SampleFormat::Int16:
        return SF_FORMAT_PCM_16;
    case SampleFormat::Int24:
        return SF_FORMAT_PCM_24;
    case SampleFormat::Int32:
        return SF_FORMAT_PCM_32;
    case SampleFormat::Float32:
        return SF_FORMAT_FLOAT;
    case SampleFormat::Float64:
        return SF_FORMAT_DOUBLE;
    }

    return 0;
}

std::uint64_t encodedBytesPerSample(const SampleFormat sampleFormat) {
    switch (sampleFormat) {
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

std::int32_t floatToInt32(const float sample) {
    constexpr auto kInt32Max = std::numeric_limits<std::int32_t>::max();
    constexpr auto kInt32Min = std::numeric_limits<std::int32_t>::min();
    const auto clamped = std::clamp(sample, -1.0F, 1.0F);

    if (clamped >= 1.0F) {
        return kInt32Max;
    }
    return static_cast<std::int32_t>(std::lrint(static_cast<double>(clamped) * -static_cast<double>(kInt32Min)));
}

} // namespace

WavWriter::~WavWriter() {
    abort();
}

bool WavWriter::open(const std::filesystem::path& path,
                     const std::uint16_t channels,
                     const std::uint32_t sampleRate,
                     const SampleFormat sampleFormat) {
    if (isOpen()) {
        lastError_ = "WAV writer is already open";
        return false;
    }
    if (path.empty() || channels == 0 || sampleRate == 0) {
        lastError_ = "WAV writer requires a path, channels, and sample rate";
        return false;
    }

    const auto encoding = sndfileEncoding(sampleFormat);
    if (encoding == 0) {
        lastError_ = "Unsupported WAV sample format";
        return false;
    }

    SF_INFO info{};
    info.samplerate = static_cast<int>(sampleRate);
    info.channels = static_cast<int>(channels);
    // The final capture length is not known when recording starts. RF64 avoids
    // crossing RIFF's 4 GiB size limit while retaining the selected encoding.
    info.format = SF_FORMAT_RF64 | encoding;

    const auto pathString = path.string();
    handle_ = sf_open(pathString.c_str(), SFM_WRITE, &info);
    if (handle_ == nullptr) {
        lastError_ = std::string("Unable to open WAV file: ") + sf_strerror(nullptr);
        return false;
    }

    path_ = path;
    channels_ = channels;
    sampleFormat_ = sampleFormat;
    provenance_ = (sampleFormat == SampleFormat::Int16 || sampleFormat == SampleFormat::Int24 ||
                   sampleFormat == SampleFormat::Int32)
                      ? ConversionProvenance::Quantized
                      : ConversionProvenance::Native;
    audioBytesWritten_ = 0;
    lastError_.clear();
    return true;
}

bool WavWriter::write(const float* const interleavedFrames, const std::size_t frameCount) {
    if (!isOpen()) {
        lastError_ = "WAV writer is not open";
        return false;
    }
    if (frameCount == 0) {
        return true;
    }
    if (interleavedFrames == nullptr) {
        lastError_ = "Cannot write null audio frames";
        return false;
    }
    if (frameCount > static_cast<std::size_t>(std::numeric_limits<sf_count_t>::max())) {
        lastError_ = "Audio frame count exceeds libsndfile limits";
        return false;
    }

    if (frameCount > std::numeric_limits<std::size_t>::max() / channels_) {
        lastError_ = "Audio sample count exceeds addressable memory";
        return false;
    }

    const auto requestedFrames = static_cast<sf_count_t>(frameCount);
    sf_count_t writtenFrames = 0;
    const auto sampleCount = frameCount * static_cast<std::size_t>(channels_);

    switch (sampleFormat_) {
    case SampleFormat::Float32:
        writtenFrames = sf_writef_float(handle_, interleavedFrames, requestedFrames);
        break;
    case SampleFormat::Float64: {
        std::vector<double> converted(sampleCount);
        std::transform(interleavedFrames, interleavedFrames + sampleCount, converted.begin(),
                       [](const float sample) { return static_cast<double>(sample); });
        writtenFrames = sf_writef_double(handle_, converted.data(), requestedFrames);
        break;
    }
    case SampleFormat::Int16:
    case SampleFormat::Int24:
    case SampleFormat::Int32: {
        std::vector<std::int32_t> converted(sampleCount);
        std::transform(interleavedFrames, interleavedFrames + sampleCount, converted.begin(), floatToInt32);
        writtenFrames = sf_writef_int(handle_, converted.data(), requestedFrames);
        break;
    }
    }

    const auto completedFrames = writtenFrames > 0 ? static_cast<std::uint64_t>(writtenFrames) : 0;
    audioBytesWritten_ += completedFrames * static_cast<std::uint64_t>(channels_) * encodedBytesPerSample(sampleFormat_);

    if (writtenFrames != requestedFrames) {
        return setErrorFromSndfile("Short WAV write (disk may be full)");
    }

    return true;
}

bool WavWriter::finalize() {
    if (!isOpen()) {
        lastError_ = "WAV writer is not open";
        return false;
    }

    sf_write_sync(handle_);
    if (sf_error(handle_) != SF_ERR_NO_ERROR) {
        return setErrorFromSndfile("Unable to flush WAV file");
    }
    if (sf_close(handle_) != 0) {
        handle_ = nullptr;
        lastError_ = "Unable to close WAV file";
        return false;
    }

    handle_ = nullptr;
    path_.clear();
    channels_ = 0;
    audioBytesWritten_ = 0;
    return true;
}

void WavWriter::abort() {
    if (handle_ != nullptr) {
        sf_close(handle_);
        handle_ = nullptr;
    }
    if (!path_.empty()) {
        std::error_code error;
        std::filesystem::remove(path_, error);
        if (error) {
            lastError_ = "Unable to remove partial WAV file: " + error.message();
        }
    }
    path_.clear();
    channels_ = 0;
    audioBytesWritten_ = 0;
}

bool WavWriter::isOpen() const noexcept {
    return handle_ != nullptr;
}

ConversionProvenance WavWriter::provenance() const noexcept {
    return provenance_;
}

const std::string& WavWriter::lastError() const noexcept {
    return lastError_;
}

bool WavWriter::setErrorFromSndfile(const char* const operation) {
    const char* const detail = sf_strerror(handle_);
    lastError_ = operation;
    if (detail != nullptr && detail[0] != '\0') {
        lastError_ += ": ";
        lastError_ += detail;
    }
    return false;
}

} // namespace abracapture::recording
