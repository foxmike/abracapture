#include "AudioDomainTypes.hpp"

namespace abracapture::exporting {

std::optional<AudioContainer> audioContainerFromString(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("wav") || normalized == QStringLiteral("wave")) {
        return AudioContainer::Wav;
    }
    if (normalized == QStringLiteral("flac")) {
        return AudioContainer::Flac;
    }
    if (normalized == QStringLiteral("ogg")) {
        return AudioContainer::Ogg;
    }
    if (normalized == QStringLiteral("mp3")) {
        return AudioContainer::Mp3;
    }
    return std::nullopt;
}

QString audioContainerName(const AudioContainer container) {
    switch (container) {
    case AudioContainer::Wav: return QStringLiteral("wav");
    case AudioContainer::Flac: return QStringLiteral("flac");
    case AudioContainer::Ogg: return QStringLiteral("ogg");
    case AudioContainer::Mp3: return QStringLiteral("mp3");
    }
    return QStringLiteral("unknown");
}

QString audioContainerFileExtension(const AudioContainer container) {
    return audioContainerName(container);
}

std::optional<AudioCodec> audioCodecFromString(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("pcm")) return AudioCodec::Pcm;
    if (normalized == QStringLiteral("flac")) return AudioCodec::Flac;
    if (normalized == QStringLiteral("mp3")) return AudioCodec::Mp3;
    if (normalized == QStringLiteral("vorbis")) return AudioCodec::Vorbis;
    return std::nullopt;
}

QString audioCodecName(const AudioCodec codec) {
    switch (codec) {
    case AudioCodec::Pcm: return QStringLiteral("pcm");
    case AudioCodec::Flac: return QStringLiteral("flac");
    case AudioCodec::Mp3: return QStringLiteral("mp3");
    case AudioCodec::Vorbis: return QStringLiteral("vorbis");
    }
    return QStringLiteral("unknown");
}

bool isLosslessCodec(const AudioCodec codec) noexcept {
    switch (codec) {
    case AudioCodec::Pcm:
    case AudioCodec::Flac:
        return true;
    case AudioCodec::Mp3:
    case AudioCodec::Vorbis:
        return false;
    }
    return false;
}

int audioSampleFormatBits(const AudioSampleFormat format) noexcept {
    switch (format) {
    case AudioSampleFormat::Unknown: return 0;
    case AudioSampleFormat::UnsignedInt8: return 8;
    case AudioSampleFormat::SignedInt16: return 16;
    case AudioSampleFormat::SignedInt24: return 24;
    case AudioSampleFormat::SignedInt32: return 32;
    case AudioSampleFormat::Float32: return 32;
    case AudioSampleFormat::Float64: return 64;
    }
    return 0;
}

QString audioSampleFormatName(const AudioSampleFormat format) {
    switch (format) {
    case AudioSampleFormat::Unknown: return QStringLiteral("unknown");
    case AudioSampleFormat::UnsignedInt8: return QStringLiteral("u8");
    case AudioSampleFormat::SignedInt16: return QStringLiteral("s16");
    case AudioSampleFormat::SignedInt24: return QStringLiteral("s24");
    case AudioSampleFormat::SignedInt32: return QStringLiteral("s32");
    case AudioSampleFormat::Float32: return QStringLiteral("f32");
    case AudioSampleFormat::Float64: return QStringLiteral("f64");
    }
    return QStringLiteral("unknown");
}

std::optional<AudioSampleFormat> audioSampleFormatFromString(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("u8")) return AudioSampleFormat::UnsignedInt8;
    if (normalized == QStringLiteral("s16")) return AudioSampleFormat::SignedInt16;
    if (normalized == QStringLiteral("s24")) return AudioSampleFormat::SignedInt24;
    if (normalized == QStringLiteral("s32")) return AudioSampleFormat::SignedInt32;
    if (normalized == QStringLiteral("f32")) return AudioSampleFormat::Float32;
    if (normalized == QStringLiteral("f64")) return AudioSampleFormat::Float64;
    return std::nullopt;
}

AudioChannelLayout AudioChannelLayout::unknown(const std::uint16_t channelCount) {
    AudioChannelLayout layout;
    layout.channelCount_ = channelCount;
    layout.explicitPositions_ = false;
    return layout;
}

AudioChannelLayout AudioChannelLayout::mono() {
    AudioChannelLayout layout;
    layout.channelCount_ = 1;
    layout.explicitPositions_ = true;
    layout.positions_ = {AudioChannelPosition::FrontCenter};
    return layout;
}

AudioChannelLayout AudioChannelLayout::stereo() {
    AudioChannelLayout layout;
    layout.channelCount_ = 2;
    layout.explicitPositions_ = true;
    layout.positions_ = {AudioChannelPosition::FrontLeft, AudioChannelPosition::FrontRight};
    return layout;
}

AudioChannelLayout AudioChannelLayout::fromPositions(QList<AudioChannelPosition> positions) {
    AudioChannelLayout layout;
    layout.channelCount_ = static_cast<std::uint16_t>(positions.size());
    layout.explicitPositions_ = true;
    layout.positions_ = std::move(positions);
    return layout;
}

std::uint16_t AudioChannelLayout::channelCount() const noexcept {
    return channelCount_;
}

bool AudioChannelLayout::hasExplicitPositions() const noexcept {
    return explicitPositions_;
}

const QList<AudioChannelPosition>& AudioChannelLayout::positions() const noexcept {
    return positions_;
}

} // namespace abracapture::exporting
