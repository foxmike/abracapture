#pragma once

#include "AudioDomainTypes.hpp"

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace abracapture::exporting {

struct AudioTargetDescriptor {
    AudioContainer container = AudioContainer::Mp3;
    AudioCodec codec = AudioCodec::Mp3;
    QString label;
    QString fileExtension;
    QString category;
    bool lossless = false;
    QList<std::uint32_t> validSampleRates;
    QList<std::uint32_t> validBitrates;
    QList<std::uint32_t> validBitDepths;
    QList<int> validCompressionLevels;
    QList<AudioSampleFormat> validSampleFormats;
    QList<int> validVorbisQualities;

    struct Defaults {
        std::uint32_t bitrate = 0;
        std::uint32_t bitDepth = 0;
        int compressionLevel = -1;
        AudioSampleFormat sampleFormat = AudioSampleFormat::Unknown;
        int vorbisQuality = -1;
    };
    Defaults defaults;
};

struct PresetDescriptor {
    QString id;
    QString label;
    AudioContainer container = AudioContainer::Mp3;
    AudioCodec codec = AudioCodec::Mp3;
    std::uint32_t bitrate = 0;
    std::uint32_t bitDepth = 0;
    int compressionLevel = -1;
    AudioSampleFormat sampleFormat = AudioSampleFormat::Unknown;
    int vorbisQuality = -1;
};

class AudioFormatCatalog {
public:
    [[nodiscard]] static const QList<AudioTargetDescriptor>& targets();
    [[nodiscard]] static std::optional<AudioTargetDescriptor> target(AudioContainer container,
                                                                     AudioCodec codec);
    [[nodiscard]] static bool isValidPairing(AudioContainer container, AudioCodec codec);
    [[nodiscard]] static const QList<PresetDescriptor>& presets();
    [[nodiscard]] static std::optional<PresetDescriptor> preset(const QString& id);

    [[nodiscard]] static QStringList formatLabels();
    [[nodiscard]] static QStringList sampleRateOptions(AudioContainer container, AudioCodec codec);
    [[nodiscard]] static QStringList bitrateOptions(AudioContainer container, AudioCodec codec);
    [[nodiscard]] static QStringList bitDepthOptions(AudioContainer container, AudioCodec codec);
    [[nodiscard]] static QStringList compressionLevelOptions(AudioContainer container,
                                                             AudioCodec codec);
    [[nodiscard]] static QStringList sampleFormatOptions(AudioContainer container, AudioCodec codec);
    [[nodiscard]] static bool supportsExplicitSampleRate(AudioContainer container, AudioCodec codec,
                                                         std::uint32_t sampleRate);
    [[nodiscard]] static QString fileExtension(AudioContainer container, AudioCodec codec);
};

} // namespace abracapture::exporting
