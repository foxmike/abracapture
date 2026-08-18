#include "AudioFormatCatalog.hpp"

namespace abracapture::exporting {

namespace {

const QList<AudioTargetDescriptor>& builtinTargets() {
    static const QList<AudioTargetDescriptor> targets = {
        {
            AudioContainer::Mp3,
            AudioCodec::Mp3,
            QStringLiteral("MP3"),
            QStringLiteral("mp3"),
            QStringLiteral("Lossy"),
            false,
            {44100, 48000},
            {128, 192, 256, 320},
            {},
            {},
            {},
            {},
            {320, 0, -1, AudioSampleFormat::Unknown, -1},
        },
        {
            AudioContainer::Flac,
            AudioCodec::Flac,
            QStringLiteral("FLAC"),
            QStringLiteral("flac"),
            QStringLiteral("Lossless"),
            true,
            {44100, 48000, 88200, 96000, 176400, 192000},
            {},
            {16, 24, 32},
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            {},
            {},
            {0, 16, 5, AudioSampleFormat::Unknown, -1},
        },
        {
            AudioContainer::Wav,
            AudioCodec::Pcm,
            QStringLiteral("WAV"),
            QStringLiteral("wav"),
            QStringLiteral("Lossless"),
            true,
            {44100, 48000, 88200, 96000, 176400, 192000},
            {},
            {},
            {},
            {AudioSampleFormat::SignedInt16, AudioSampleFormat::SignedInt24,
             AudioSampleFormat::SignedInt32, AudioSampleFormat::Float32},
            {},
            {0, 0, -1, AudioSampleFormat::SignedInt16, -1},
        },
        {
            AudioContainer::Ogg,
            AudioCodec::Vorbis,
            QStringLiteral("Ogg Vorbis"),
            QStringLiteral("ogg"),
            QStringLiteral("Lossy"),
            false,
            {44100, 48000},
            {},
            {},
            {},
            {},
            {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
            {0, 0, -1, AudioSampleFormat::Unknown, 5},
        },
    };
    return targets;
}

const QList<PresetDescriptor>& builtinPresets() {
    static const QList<PresetDescriptor> presets = {
        {QStringLiteral("mp3_320"), QStringLiteral("MP3 320 kbps"),
         AudioContainer::Mp3, AudioCodec::Mp3, 320, 0, -1, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("mp3_256"), QStringLiteral("MP3 256 kbps"),
         AudioContainer::Mp3, AudioCodec::Mp3, 256, 0, -1, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("flac_16"), QStringLiteral("FLAC 16-bit"),
         AudioContainer::Flac, AudioCodec::Flac, 0, 16, 5, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("flac_24"), QStringLiteral("FLAC 24-bit"),
         AudioContainer::Flac, AudioCodec::Flac, 0, 24, 5, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("wav_24"), QStringLiteral("WAV 24-bit"),
         AudioContainer::Wav, AudioCodec::Pcm, 0, 0, -1, AudioSampleFormat::SignedInt24, -1},
        {QStringLiteral("preserve_lossless"), QStringLiteral("WAV Float (Preserve)"),
         AudioContainer::Wav, AudioCodec::Pcm, 0, 0, -1, AudioSampleFormat::Float32, -1},
    };
    return presets;
}

} // namespace

const QList<AudioTargetDescriptor>& AudioFormatCatalog::targets() {
    return builtinTargets();
}

std::optional<AudioTargetDescriptor> AudioFormatCatalog::target(const AudioContainer container,
                                                                 const AudioCodec codec) {
    for (const auto& descriptor : builtinTargets()) {
        if (descriptor.container == container && descriptor.codec == codec) {
            return descriptor;
        }
    }
    return std::nullopt;
}

bool AudioFormatCatalog::isValidPairing(const AudioContainer container, const AudioCodec codec) {
    return target(container, codec).has_value();
}

const QList<PresetDescriptor>& AudioFormatCatalog::presets() {
    return builtinPresets();
}

std::optional<PresetDescriptor> AudioFormatCatalog::preset(const QString& id) {
    for (const auto& descriptor : builtinPresets()) {
        if (descriptor.id == id) {
            return descriptor;
        }
    }
    return std::nullopt;
}

QStringList AudioFormatCatalog::formatLabels() {
    QStringList labels;
    for (const auto& descriptor : builtinTargets()) {
        labels.append(descriptor.label);
    }
    return labels;
}

QStringList AudioFormatCatalog::sampleRateOptions(const AudioContainer container,
                                                   const AudioCodec codec) {
    const auto descriptor = target(container, codec);
    if (!descriptor.has_value()) {
        return {};
    }
    QStringList options;
    options.append(QStringLiteral("preserve"));
    for (const std::uint32_t rate : descriptor->validSampleRates) {
        options.append(QString::number(rate));
    }
    return options;
}

QStringList AudioFormatCatalog::bitrateOptions(const AudioContainer container,
                                                const AudioCodec codec) {
    const auto descriptor = target(container, codec);
    if (!descriptor.has_value()) {
        return {};
    }
    QStringList options;
    for (const std::uint32_t bitrate : descriptor->validBitrates) {
        options.append(QString::number(bitrate));
    }
    return options;
}

QStringList AudioFormatCatalog::bitDepthOptions(const AudioContainer container,
                                                 const AudioCodec codec) {
    const auto descriptor = target(container, codec);
    if (!descriptor.has_value()) {
        return {};
    }
    QStringList options;
    for (const std::uint32_t depth : descriptor->validBitDepths) {
        options.append(QString::number(depth));
    }
    return options;
}

QStringList AudioFormatCatalog::compressionLevelOptions(const AudioContainer container,
                                                         const AudioCodec codec) {
    const auto descriptor = target(container, codec);
    if (!descriptor.has_value()) {
        return {};
    }
    QStringList options;
    for (const int level : descriptor->validCompressionLevels) {
        options.append(QString::number(level));
    }
    return options;
}

QStringList AudioFormatCatalog::sampleFormatOptions(const AudioContainer container,
                                                     const AudioCodec codec) {
    const auto descriptor = target(container, codec);
    if (!descriptor.has_value()) {
        return {};
    }
    QStringList options;
    for (const AudioSampleFormat format : descriptor->validSampleFormats) {
        options.append(audioSampleFormatName(format));
    }
    return options;
}

bool AudioFormatCatalog::supportsExplicitSampleRate(const AudioContainer container,
                                                     const AudioCodec codec,
                                                     const std::uint32_t sampleRate) {
    const auto descriptor = target(container, codec);
    if (!descriptor.has_value()) {
        return false;
    }
    return descriptor->validSampleRates.contains(sampleRate);
}

QString AudioFormatCatalog::fileExtension(const AudioContainer container, const AudioCodec codec) {
    const auto descriptor = target(container, codec);
    if (descriptor.has_value()) {
        return descriptor->fileExtension;
    }
    return audioContainerFileExtension(container);
}

} // namespace abracapture::exporting
