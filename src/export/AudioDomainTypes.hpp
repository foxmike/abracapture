#pragma once

#include <QList>
#include <QString>

#include <cstdint>
#include <optional>

namespace abracapture::exporting {

enum class AudioContainer : std::uint8_t {
    Wav,
    Flac,
    Ogg,
    Mp3,
};

[[nodiscard]] std::optional<AudioContainer> audioContainerFromString(const QString& name);
[[nodiscard]] QString audioContainerName(AudioContainer container);
[[nodiscard]] QString audioContainerFileExtension(AudioContainer container);

enum class AudioCodec : std::uint8_t {
    Pcm,
    Flac,
    Mp3,
    Vorbis,
};

[[nodiscard]] std::optional<AudioCodec> audioCodecFromString(const QString& name);
[[nodiscard]] QString audioCodecName(AudioCodec codec);
[[nodiscard]] bool isLosslessCodec(AudioCodec codec) noexcept;

enum class AudioSampleFormat : std::uint8_t {
    Unknown,
    UnsignedInt8,
    SignedInt16,
    SignedInt24,
    SignedInt32,
    Float32,
    Float64,
};

[[nodiscard]] int audioSampleFormatBits(AudioSampleFormat format) noexcept;
[[nodiscard]] QString audioSampleFormatName(AudioSampleFormat format);
[[nodiscard]] std::optional<AudioSampleFormat> audioSampleFormatFromString(const QString& name);

enum class AudioChannelPosition : std::uint8_t {
    FrontLeft,
    FrontRight,
    FrontCenter,
    Lfe,
    RearLeft,
    RearRight,
    SideLeft,
    SideRight,
};

class AudioChannelLayout {
public:
    AudioChannelLayout() = default;

    [[nodiscard]] static AudioChannelLayout unknown(std::uint16_t channelCount);
    [[nodiscard]] static AudioChannelLayout mono();
    [[nodiscard]] static AudioChannelLayout stereo();
    [[nodiscard]] static AudioChannelLayout fromPositions(QList<AudioChannelPosition> positions);

    [[nodiscard]] std::uint16_t channelCount() const noexcept;
    [[nodiscard]] bool hasExplicitPositions() const noexcept;
    [[nodiscard]] const QList<AudioChannelPosition>& positions() const noexcept;

    friend bool operator==(const AudioChannelLayout& a, const AudioChannelLayout& b) noexcept {
        return a.channelCount_ == b.channelCount_ &&
               a.explicitPositions_ == b.explicitPositions_ &&
               a.positions_ == b.positions_;
    }
    friend bool operator!=(const AudioChannelLayout& a, const AudioChannelLayout& b) noexcept {
        return !(a == b);
    }

private:
    QList<AudioChannelPosition> positions_;
    std::uint16_t channelCount_ = 0;
    bool explicitPositions_ = false;
};

struct AudioStreamFormat {
    std::uint32_t sampleRate = 0;
    AudioSampleFormat sampleFormat = AudioSampleFormat::Unknown;
    AudioChannelLayout channelLayout;

    friend bool operator==(const AudioStreamFormat& a, const AudioStreamFormat& b) noexcept {
        return a.sampleRate == b.sampleRate &&
               a.sampleFormat == b.sampleFormat &&
               a.channelLayout == b.channelLayout;
    }
    friend bool operator!=(const AudioStreamFormat& a, const AudioStreamFormat& b) noexcept {
        return !(a == b);
    }
};

} // namespace abracapture::exporting
