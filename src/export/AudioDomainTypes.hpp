#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace abracapture::exporting {

enum class AudioContainer : std::uint8_t {
    Wav,
    Flac,
    Ogg,
    Mp3,
    Aiff,
    M4a,
    WavPack,
    Caf,
    Opus,
    Ac3,
    Eac3,
};

[[nodiscard]] std::optional<AudioContainer> audioContainerFromString(const QString& name);
[[nodiscard]] QString audioContainerName(AudioContainer container);
[[nodiscard]] QString audioContainerFileExtension(AudioContainer container);

enum class AudioCodec : std::uint8_t {
    Pcm,
    Flac,
    Mp3,
    Vorbis,
    Alac,
    Aac,
    WavPack,
    Opus,
    Ac3,
    Eac3,
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

[[nodiscard]] QString audioChannelPositionName(AudioChannelPosition position);
[[nodiscard]] std::optional<AudioChannelPosition> audioChannelPositionFromString(const QString& name);

class AudioChannelLayout {
public:
    AudioChannelLayout() = default;

    [[nodiscard]] static AudioChannelLayout unknown(std::uint16_t channelCount);
    [[nodiscard]] static AudioChannelLayout mono();
    [[nodiscard]] static AudioChannelLayout stereo();
    [[nodiscard]] static AudioChannelLayout surround51();
    [[nodiscard]] static AudioChannelLayout surround71();
    [[nodiscard]] static AudioChannelLayout fromPositions(QList<AudioChannelPosition> positions);
    [[nodiscard]] static AudioChannelLayout fromPositionNames(const QStringList& names,
                                                              std::uint16_t fallbackCount = 0);

    [[nodiscard]] std::uint16_t channelCount() const noexcept;
    [[nodiscard]] bool hasExplicitPositions() const noexcept;
    [[nodiscard]] const QList<AudioChannelPosition>& positions() const noexcept;

    [[nodiscard]] bool isMono() const noexcept;
    [[nodiscard]] bool isStereo() const noexcept;
    [[nodiscard]] bool is5Point1() const noexcept;
    [[nodiscard]] bool is7Point1() const noexcept;

    [[nodiscard]] QString displayName() const;
    [[nodiscard]] QString ffmpegLayoutName() const;
    [[nodiscard]] QStringList positionNames() const;

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
