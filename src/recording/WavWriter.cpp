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

// Convert our exporting::AudioChannelPosition into the closest SF_CHANNEL_MAP_*
// value. Returns -1 when the position has no direct libsndfile equivalent.
int sndfileChannelFor(const abracapture::exporting::AudioChannelPosition position) {
    switch (position) {
    case abracapture::exporting::AudioChannelPosition::FrontLeft:   return SF_CHANNEL_MAP_FRONT_LEFT;
    case abracapture::exporting::AudioChannelPosition::FrontRight:  return SF_CHANNEL_MAP_FRONT_RIGHT;
    case abracapture::exporting::AudioChannelPosition::FrontCenter: return SF_CHANNEL_MAP_FRONT_CENTER;
    case abracapture::exporting::AudioChannelPosition::Lfe:         return SF_CHANNEL_MAP_LFE;
    case abracapture::exporting::AudioChannelPosition::RearLeft:    return SF_CHANNEL_MAP_REAR_LEFT;
    case abracapture::exporting::AudioChannelPosition::RearRight:   return SF_CHANNEL_MAP_REAR_RIGHT;
    case abracapture::exporting::AudioChannelPosition::SideLeft:    return SF_CHANNEL_MAP_SIDE_LEFT;
    case abracapture::exporting::AudioChannelPosition::SideRight:   return SF_CHANNEL_MAP_SIDE_RIGHT;
    }
    return -1;
}

// Apply the negotiated channel layout to a freshly-opened libsndfile handle.
// Unknown layouts are handled by the caller, which rejects a backend that
// would infer a speaker map from channel count alone.
bool applyChannelLayout(SNDFILE* handle, const std::uint16_t channels,
                        const abracapture::exporting::AudioChannelLayout& layout,
                        std::string& error) {
    if (handle == nullptr) {
        return true;
    }
    if (!layout.hasExplicitPositions()) {
        return true;
    }
    if (layout.channelCount() != channels) {
        error = "WAV channel layout count does not match the audio channel count";
        return false;
    }
    const auto& positions = layout.positions();
    if (static_cast<std::uint16_t>(positions.size()) != channels) {
        error = "WAV channel layout does not contain one position per channel";
        return false;
    }
    std::vector<int> channelMap;
    channelMap.reserve(channels);
    for (const auto pos : positions) {
        const int code = sndfileChannelFor(pos);
        if (code < 0) {
            error = "WAV channel layout contains a speaker position unsupported by libsndfile";
            return false;
        }
        channelMap.push_back(code);
    }

    // libsndfile documents this command as boolean, but its RF64/WAVEX
    // implementation returns zero after accepting the map. Treat the setter's
    // return value as advisory and verify the persisted map through the getter
    // before accepting the file. The vector remains alive for both calls.
    static_cast<void>(sf_command(handle, SFC_SET_CHANNEL_MAP_INFO, channelMap.data(),
                                 static_cast<int>(sizeof(int) * channelMap.size())));

    // Mono and stereo already have unambiguous PCM identities and retain the
    // existing writer behavior across libsndfile versions. For multichannel
    // files, accepting a channel count without a matching reopened map would
    // silently turn an unknown or custom layout into a guessed one.
    if (channels <= 2) {
        return true;
    }

    std::vector<int> persistedMap(channelMap.size(), SF_CHANNEL_MAP_INVALID);
    if (sf_command(handle, SFC_GET_CHANNEL_MAP_INFO, persistedMap.data(),
                   static_cast<int>(sizeof(int) * persistedMap.size())) != SF_TRUE) {
        error = "WAV/RF64 cannot persist the requested multichannel speaker map";
        return false;
    }

    const auto sameSpeaker = [](const int expected, const int actual) {
        if (expected == actual) {
            return true;
        }
        return (expected == SF_CHANNEL_MAP_FRONT_LEFT && actual == SF_CHANNEL_MAP_LEFT) ||
               (expected == SF_CHANNEL_MAP_FRONT_RIGHT && actual == SF_CHANNEL_MAP_RIGHT) ||
               (expected == SF_CHANNEL_MAP_FRONT_CENTER && actual == SF_CHANNEL_MAP_CENTER);
    };
    for (std::size_t index = 0; index < channelMap.size(); ++index) {
        if (!sameSpeaker(channelMap[index], persistedMap[index])) {
            error = "WAV/RF64 cannot persist the requested multichannel speaker map";
            return false;
        }
    }
    return true;
}

} // namespace

WavWriter::~WavWriter() {
    abort();
}

bool WavWriter::open(const std::filesystem::path& path,
                     const std::uint16_t channels,
                     const std::uint32_t sampleRate,
                     const SampleFormat sampleFormat) {
    return open(path, channels, sampleRate, sampleFormat,
                abracapture::exporting::AudioChannelLayout{});
}

bool WavWriter::open(const std::filesystem::path& path,
                     const std::uint16_t channels,
                     const std::uint32_t sampleRate,
                     const SampleFormat sampleFormat,
                     const abracapture::exporting::AudioChannelLayout& layout) {
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
    if (!layout.hasExplicitPositions() && channels > 2) {
        // This libsndfile/WAVEX path writes a channel mask from the count when
        // the header is finalized, even though GET_CHANNEL_MAP_INFO is empty
        // immediately after open. Refuse the capture rather than allow
        // Unknown(6) or Unknown(8) to become a guessed surround layout.
        lastError_ = "WAV/RF64 cannot preserve an unknown multichannel layout";
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

    std::string layoutError;
    if (!applyChannelLayout(handle_, channels, layout, layoutError)) {
        lastError_ = std::move(layoutError);
        sf_close(handle_);
        handle_ = nullptr;
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

bool WavWriter::open(const std::filesystem::path& path, const AudioFormat& format) {
    if (format.channels == 0 || format.sampleRate == 0) {
        lastError_ = "WAV writer requires channels and sample rate";
        return false;
    }
    return open(path, format.channels, format.sampleRate, format.sampleFormat, format.channelLayout);
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
