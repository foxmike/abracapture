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
    if (normalized == QStringLiteral("aiff") || normalized == QStringLiteral("aif")) {
        return AudioContainer::Aiff;
    }
    if (normalized == QStringLiteral("m4a") || normalized == QStringLiteral("mp4")) {
        return AudioContainer::M4a;
    }
    if (normalized == QStringLiteral("wavpack") || normalized == QStringLiteral("wv")) {
        return AudioContainer::WavPack;
    }
    if (normalized == QStringLiteral("caf")) {
        return AudioContainer::Caf;
    }
    if (normalized == QStringLiteral("opus")) {
        return AudioContainer::Opus;
    }
    if (normalized == QStringLiteral("ac3") || normalized == QStringLiteral("ac-3")) {
        return AudioContainer::Ac3;
    }
    if (normalized == QStringLiteral("eac3") || normalized == QStringLiteral("e-ac-3") ||
        normalized == QStringLiteral("ec3")) {
        return AudioContainer::Eac3;
    }
    return std::nullopt;
}

QString audioContainerName(const AudioContainer container) {
    switch (container) {
    case AudioContainer::Wav: return QStringLiteral("wav");
    case AudioContainer::Flac: return QStringLiteral("flac");
    case AudioContainer::Ogg: return QStringLiteral("ogg");
    case AudioContainer::Mp3: return QStringLiteral("mp3");
    case AudioContainer::Aiff: return QStringLiteral("aiff");
    case AudioContainer::M4a: return QStringLiteral("m4a");
    case AudioContainer::WavPack: return QStringLiteral("wavpack");
    case AudioContainer::Caf: return QStringLiteral("caf");
    case AudioContainer::Opus: return QStringLiteral("opus");
    case AudioContainer::Ac3: return QStringLiteral("ac3");
    case AudioContainer::Eac3: return QStringLiteral("eac3");
    }
    return QStringLiteral("unknown");
}

QString audioContainerFileExtension(const AudioContainer container) {
    switch (container) {
    case AudioContainer::Wav: return QStringLiteral("wav");
    case AudioContainer::Flac: return QStringLiteral("flac");
    case AudioContainer::Ogg: return QStringLiteral("ogg");
    case AudioContainer::Mp3: return QStringLiteral("mp3");
    case AudioContainer::Aiff: return QStringLiteral("aiff");
    case AudioContainer::M4a: return QStringLiteral("m4a");
    case AudioContainer::WavPack: return QStringLiteral("wv");
    case AudioContainer::Caf: return QStringLiteral("caf");
    case AudioContainer::Opus: return QStringLiteral("opus");
    case AudioContainer::Ac3: return QStringLiteral("ac3");
    case AudioContainer::Eac3: return QStringLiteral("eac3");
    }
    return QStringLiteral("unknown");
}

std::optional<AudioCodec> audioCodecFromString(const QString& name) {
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("pcm")) return AudioCodec::Pcm;
    if (normalized == QStringLiteral("flac")) return AudioCodec::Flac;
    if (normalized == QStringLiteral("mp3")) return AudioCodec::Mp3;
    if (normalized == QStringLiteral("vorbis")) return AudioCodec::Vorbis;
    if (normalized == QStringLiteral("alac")) return AudioCodec::Alac;
    if (normalized == QStringLiteral("aac")) return AudioCodec::Aac;
    if (normalized == QStringLiteral("wavpack") || normalized == QStringLiteral("wv")) return AudioCodec::WavPack;
    if (normalized == QStringLiteral("opus")) return AudioCodec::Opus;
    if (normalized == QStringLiteral("ac3") || normalized == QStringLiteral("ac-3")) return AudioCodec::Ac3;
    if (normalized == QStringLiteral("eac3") || normalized == QStringLiteral("e-ac-3") ||
        normalized == QStringLiteral("ec3")) return AudioCodec::Eac3;
    return std::nullopt;
}

QString audioCodecName(const AudioCodec codec) {
    switch (codec) {
    case AudioCodec::Pcm: return QStringLiteral("pcm");
    case AudioCodec::Flac: return QStringLiteral("flac");
    case AudioCodec::Mp3: return QStringLiteral("mp3");
    case AudioCodec::Vorbis: return QStringLiteral("vorbis");
    case AudioCodec::Alac: return QStringLiteral("alac");
    case AudioCodec::Aac: return QStringLiteral("aac");
    case AudioCodec::WavPack: return QStringLiteral("wavpack");
    case AudioCodec::Opus: return QStringLiteral("opus");
    case AudioCodec::Ac3: return QStringLiteral("ac3");
    case AudioCodec::Eac3: return QStringLiteral("eac3");
    }
    return QStringLiteral("unknown");
}

bool isLosslessCodec(const AudioCodec codec) noexcept {
    switch (codec) {
    case AudioCodec::Pcm:
    case AudioCodec::Flac:
    case AudioCodec::Alac:
    case AudioCodec::WavPack:
        return true;
    case AudioCodec::Mp3:
    case AudioCodec::Vorbis:
    case AudioCodec::Aac:
    case AudioCodec::Opus:
    case AudioCodec::Ac3:
    case AudioCodec::Eac3:
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

QString audioChannelPositionName(const AudioChannelPosition position) {
    switch (position) {
    case AudioChannelPosition::FrontLeft: return QStringLiteral("FL");
    case AudioChannelPosition::FrontRight: return QStringLiteral("FR");
    case AudioChannelPosition::FrontCenter: return QStringLiteral("FC");
    case AudioChannelPosition::Lfe: return QStringLiteral("LFE");
    case AudioChannelPosition::RearLeft: return QStringLiteral("RL");
    case AudioChannelPosition::RearRight: return QStringLiteral("RR");
    case AudioChannelPosition::SideLeft: return QStringLiteral("SL");
    case AudioChannelPosition::SideRight: return QStringLiteral("SR");
    }
    return QStringLiteral("UNKNOWN");
}

std::optional<AudioChannelPosition> audioChannelPositionFromString(const QString& name) {
    const QString normalized = name.trimmed().toUpper();
    if (normalized == QStringLiteral("FL") || normalized == QStringLiteral("FRONT_LEFT") ||
        normalized == QStringLiteral("FRONT LEFT") || normalized == QStringLiteral("LEFT")) {
        return AudioChannelPosition::FrontLeft;
    }
    if (normalized == QStringLiteral("FR") || normalized == QStringLiteral("FRONT_RIGHT") ||
        normalized == QStringLiteral("FRONT RIGHT") || normalized == QStringLiteral("RIGHT")) {
        return AudioChannelPosition::FrontRight;
    }
    if (normalized == QStringLiteral("FC") || normalized == QStringLiteral("FRONT_CENTER") ||
        normalized == QStringLiteral("FRONT CENTER") || normalized == QStringLiteral("CENTER")) {
        return AudioChannelPosition::FrontCenter;
    }
    if (normalized == QStringLiteral("LFE") || normalized == QStringLiteral("SUBWOOFER")) {
        return AudioChannelPosition::Lfe;
    }
    if (normalized == QStringLiteral("RL") || normalized == QStringLiteral("REAR_LEFT") ||
        normalized == QStringLiteral("REAR LEFT") || normalized == QStringLiteral("BACK_LEFT") ||
        normalized == QStringLiteral("BL")) {
        return AudioChannelPosition::RearLeft;
    }
    if (normalized == QStringLiteral("RR") || normalized == QStringLiteral("REAR_RIGHT") ||
        normalized == QStringLiteral("REAR RIGHT") || normalized == QStringLiteral("BACK_RIGHT") ||
        normalized == QStringLiteral("BR")) {
        return AudioChannelPosition::RearRight;
    }
    if (normalized == QStringLiteral("SL") || normalized == QStringLiteral("SIDE_LEFT") ||
        normalized == QStringLiteral("SIDE LEFT")) {
        return AudioChannelPosition::SideLeft;
    }
    if (normalized == QStringLiteral("SR") || normalized == QStringLiteral("SIDE_RIGHT") ||
        normalized == QStringLiteral("SIDE RIGHT")) {
        return AudioChannelPosition::SideRight;
    }
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

AudioChannelLayout AudioChannelLayout::surround51() {
    AudioChannelLayout layout;
    layout.channelCount_ = 6;
    layout.explicitPositions_ = true;
    layout.positions_ = {
        AudioChannelPosition::FrontLeft,
        AudioChannelPosition::FrontRight,
        AudioChannelPosition::FrontCenter,
        AudioChannelPosition::Lfe,
        AudioChannelPosition::RearLeft,
        AudioChannelPosition::RearRight,
    };
    return layout;
}

AudioChannelLayout AudioChannelLayout::surround71() {
    AudioChannelLayout layout;
    layout.channelCount_ = 8;
    layout.explicitPositions_ = true;
    layout.positions_ = {
        AudioChannelPosition::FrontLeft,
        AudioChannelPosition::FrontRight,
        AudioChannelPosition::FrontCenter,
        AudioChannelPosition::Lfe,
        AudioChannelPosition::RearLeft,
        AudioChannelPosition::RearRight,
        AudioChannelPosition::SideLeft,
        AudioChannelPosition::SideRight,
    };
    return layout;
}

AudioChannelLayout AudioChannelLayout::fromPositions(QList<AudioChannelPosition> positions) {
    AudioChannelLayout layout;
    layout.channelCount_ = static_cast<std::uint16_t>(positions.size());
    layout.explicitPositions_ = true;
    layout.positions_ = std::move(positions);
    return layout;
}

AudioChannelLayout AudioChannelLayout::fromPositionNames(const QStringList& names,
                                                          const std::uint16_t fallbackCount) {
    if (names.isEmpty()) {
        return unknown(fallbackCount);
    }
    QList<AudioChannelPosition> posList;
    posList.reserve(names.size());
    for (const QString& name : names) {
        const auto pos = audioChannelPositionFromString(name);
        if (!pos.has_value()) {
            return unknown(fallbackCount > 0 ? fallbackCount : static_cast<std::uint16_t>(names.size()));
        }
        posList.append(*pos);
    }
    return fromPositions(std::move(posList));
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

bool AudioChannelLayout::isMono() const noexcept {
    return explicitPositions_ && channelCount_ == 1 &&
           !positions_.isEmpty() && positions_.first() == AudioChannelPosition::FrontCenter;
}

bool AudioChannelLayout::isStereo() const noexcept {
    return explicitPositions_ && channelCount_ == 2 && positions_.size() == 2 &&
           positions_.at(0) == AudioChannelPosition::FrontLeft &&
           positions_.at(1) == AudioChannelPosition::FrontRight;
}

bool AudioChannelLayout::is5Point1() const noexcept {
    if (!explicitPositions_ || channelCount_ != 6 || positions_.size() != 6) {
        return false;
    }
    const bool hasFl = positions_.contains(AudioChannelPosition::FrontLeft);
    const bool hasFr = positions_.contains(AudioChannelPosition::FrontRight);
    const bool hasFc = positions_.contains(AudioChannelPosition::FrontCenter);
    const bool hasLfe = positions_.contains(AudioChannelPosition::Lfe);
    const bool hasSurrounds = (positions_.contains(AudioChannelPosition::RearLeft) &&
                               positions_.contains(AudioChannelPosition::RearRight)) ||
                              (positions_.contains(AudioChannelPosition::SideLeft) &&
                               positions_.contains(AudioChannelPosition::SideRight));
    return hasFl && hasFr && hasFc && hasLfe && hasSurrounds;
}

bool AudioChannelLayout::is7Point1() const noexcept {
    if (!explicitPositions_ || channelCount_ != 8 || positions_.size() != 8) {
        return false;
    }
    return positions_.contains(AudioChannelPosition::FrontLeft) &&
           positions_.contains(AudioChannelPosition::FrontRight) &&
           positions_.contains(AudioChannelPosition::FrontCenter) &&
           positions_.contains(AudioChannelPosition::Lfe) &&
           positions_.contains(AudioChannelPosition::RearLeft) &&
           positions_.contains(AudioChannelPosition::RearRight) &&
           positions_.contains(AudioChannelPosition::SideLeft) &&
           positions_.contains(AudioChannelPosition::SideRight);
}

QString AudioChannelLayout::displayName() const {
    if (!explicitPositions_) {
        return QStringLiteral("%1 channels — layout unknown").arg(channelCount_);
    }
    if (isMono()) {
        return QStringLiteral("Mono");
    }
    if (isStereo()) {
        return QStringLiteral("Stereo");
    }
    if (is5Point1()) {
        return QStringLiteral("5.1");
    }
    if (is7Point1()) {
        return QStringLiteral("7.1");
    }
    QStringList names;
    names.reserve(positions_.size());
    for (const auto pos : positions_) {
        names.append(audioChannelPositionName(pos));
    }
    return names.join(QLatin1Char(' '));
}

QString AudioChannelLayout::ffmpegLayoutName() const {
    if (!explicitPositions_) {
        return {};
    }
    if (isMono()) {
        return QStringLiteral("mono");
    }
    if (isStereo()) {
        return QStringLiteral("stereo");
    }
    if (is5Point1()) {
        if (positions_.contains(AudioChannelPosition::SideLeft) &&
            positions_.contains(AudioChannelPosition::SideRight) &&
            !positions_.contains(AudioChannelPosition::RearLeft)) {
            return QStringLiteral("5.1(side)");
        }
        return QStringLiteral("5.1");
    }
    if (is7Point1()) {
        return QStringLiteral("7.1");
    }
    return {};
}

QStringList AudioChannelLayout::positionNames() const {
    if (!explicitPositions_) {
        return {};
    }
    QStringList names;
    names.reserve(positions_.size());
    for (const auto pos : positions_) {
        names.append(audioChannelPositionName(pos));
    }
    return names;
}

} // namespace abracapture::exporting
