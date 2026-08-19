#include "AudioFormatCatalog.hpp"
#include "ExportTypes.hpp"

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
        {
            AudioContainer::Aiff,
            AudioCodec::Pcm,
            QStringLiteral("AIFF"),
            QStringLiteral("aiff"),
            QStringLiteral("Lossless"),
            true,
            {44100, 48000, 88200, 96000, 176400, 192000},
            {},
            {},
            {},
            {AudioSampleFormat::SignedInt16, AudioSampleFormat::SignedInt24,
             AudioSampleFormat::SignedInt32, AudioSampleFormat::Float32},
            {},
            {0, 0, -1, AudioSampleFormat::SignedInt24, -1},
        },
        {
            AudioContainer::M4a,
            AudioCodec::Alac,
            QStringLiteral("ALAC (M4A)"),
            QStringLiteral("m4a"),
            QStringLiteral("Lossless"),
            true,
            {44100, 48000, 88200, 96000, 176400, 192000},
            {},
            {16, 24},
            {},
            {},
            {},
            {0, 24, -1, AudioSampleFormat::Unknown, -1},
        },
        {
            AudioContainer::M4a,
            AudioCodec::Aac,
            QStringLiteral("AAC (M4A)"),
            QStringLiteral("m4a"),
            QStringLiteral("Lossy"),
            false,
            {44100, 48000, 88200, 96000},
            {128, 192, 256, 320},
            {},
            {},
            {},
            {},
            {256, 0, -1, AudioSampleFormat::Unknown, -1},
        },
        {
            AudioContainer::WavPack,
            AudioCodec::WavPack,
            QStringLiteral("WavPack"),
            QStringLiteral("wv"),
            QStringLiteral("Lossless"),
            true,
            {44100, 48000, 88200, 96000, 176400, 192000},
            {},
            {16, 24, 32},
            {},
            {},
            {},
            {0, 24, -1, AudioSampleFormat::Unknown, -1},
        },
        {
            AudioContainer::Caf,
            AudioCodec::Pcm,
            QStringLiteral("CAF"),
            QStringLiteral("caf"),
            QStringLiteral("Lossless"),
            true,
            {44100, 48000, 88200, 96000, 176400, 192000},
            {},
            {},
            {},
            {AudioSampleFormat::SignedInt16, AudioSampleFormat::SignedInt24,
             AudioSampleFormat::SignedInt32, AudioSampleFormat::Float32},
            {},
            {0, 0, -1, AudioSampleFormat::SignedInt24, -1},
        },
        {
            AudioContainer::Opus,
            AudioCodec::Opus,
            QStringLiteral("Opus"),
            QStringLiteral("opus"),
            QStringLiteral("Lossy"),
            false,
            {48000},
            {96, 128, 160, 192, 256, 320},
            {},
            {},
            {},
            {},
            {160, 0, -1, AudioSampleFormat::Unknown, -1},
        },
        {
            AudioContainer::Ac3,
            AudioCodec::Ac3,
            QStringLiteral("AC-3"),
            QStringLiteral("ac3"),
            QStringLiteral("Surround"),
            false,
            {32000, 44100, 48000},
            {192, 224, 384, 448, 640},
            {},
            {},
            {},
            {},
            {640, 0, -1, AudioSampleFormat::Unknown, -1},
        },
        {
            AudioContainer::Eac3,
            AudioCodec::Eac3,
            QStringLiteral("E-AC-3"),
            QStringLiteral("eac3"),
            QStringLiteral("Surround"),
            false,
            {32000, 44100, 48000},
            {224, 384, 448, 640, 768, 1024},
            {},
            {},
            {},
            {},
            {768, 0, -1, AudioSampleFormat::Unknown, -1},
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
        {QStringLiteral("aiff_24"), QStringLiteral("AIFF 24-bit"),
         AudioContainer::Aiff, AudioCodec::Pcm, 0, 0, -1, AudioSampleFormat::SignedInt24, -1},
        {QStringLiteral("alac_lossless"), QStringLiteral("ALAC Lossless"),
         AudioContainer::M4a, AudioCodec::Alac, 0, 24, -1, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("wavpack_lossless"), QStringLiteral("WavPack Lossless"),
         AudioContainer::WavPack, AudioCodec::WavPack, 0, 24, -1, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("caf_24"), QStringLiteral("CAF 24-bit"),
         AudioContainer::Caf, AudioCodec::Pcm, 0, 0, -1, AudioSampleFormat::SignedInt24, -1},
        {QStringLiteral("aac_256"), QStringLiteral("AAC 256 kbps"),
         AudioContainer::M4a, AudioCodec::Aac, 256, 0, -1, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("opus_160"), QStringLiteral("Opus 160 kbps"),
         AudioContainer::Opus, AudioCodec::Opus, 160, 0, -1, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("ac3_51"), QStringLiteral("AC-3 5.1 (640 kbps)"),
         AudioContainer::Ac3, AudioCodec::Ac3, 640, 0, -1, AudioSampleFormat::Unknown, -1},
        {QStringLiteral("eac3_51"), QStringLiteral("E-AC-3 5.1 (768 kbps)"),
         AudioContainer::Eac3, AudioCodec::Eac3, 768, 0, -1, AudioSampleFormat::Unknown, -1},
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

std::optional<AudioTargetDescriptor> AudioFormatCatalog::findTarget(const QString& nameOrExtension) {
    const QString normalized = nameOrExtension.trimmed().toLower();
    if (normalized.isEmpty()) {
        return std::nullopt;
    }
    if (normalized == QStringLiteral("wave") || normalized == QStringLiteral("wav")) {
        return target(AudioContainer::Wav, AudioCodec::Pcm);
    }
    if (normalized == QStringLiteral("vorbis") || normalized == QStringLiteral("ogg")) {
        return target(AudioContainer::Ogg, AudioCodec::Vorbis);
    }
    if (normalized == QStringLiteral("aiff") || normalized == QStringLiteral("aif")) {
        return target(AudioContainer::Aiff, AudioCodec::Pcm);
    }
    if (normalized == QStringLiteral("alac")) {
        return target(AudioContainer::M4a, AudioCodec::Alac);
    }
    if (normalized == QStringLiteral("aac")) {
        return target(AudioContainer::M4a, AudioCodec::Aac);
    }
    if (normalized == QStringLiteral("wavpack") || normalized == QStringLiteral("wv")) {
        return target(AudioContainer::WavPack, AudioCodec::WavPack);
    }
    // M4A is a container shared by ALAC and AAC; require the codec-specific
    // lookup name so the CLI never silently chooses one.
    if (normalized == QStringLiteral("m4a") || normalized == QStringLiteral("mp4")) {
        return std::nullopt;
    }
    if (normalized == QStringLiteral("caf")) {
        return target(AudioContainer::Caf, AudioCodec::Pcm);
    }
    if (normalized == QStringLiteral("opus")) {
        return target(AudioContainer::Opus, AudioCodec::Opus);
    }
    if (normalized == QStringLiteral("ac3") || normalized == QStringLiteral("ac-3")) {
        return target(AudioContainer::Ac3, AudioCodec::Ac3);
    }
    if (normalized == QStringLiteral("eac3") || normalized == QStringLiteral("e-ac-3") ||
        normalized == QStringLiteral("ec3")) {
        return target(AudioContainer::Eac3, AudioCodec::Eac3);
    }
    for (const auto& descriptor : builtinTargets()) {
        if (QString::compare(descriptor.fileExtension, normalized, Qt::CaseInsensitive) == 0 ||
            QString::compare(audioContainerName(descriptor.container), normalized, Qt::CaseInsensitive) == 0 ||
            QString::compare(audioCodecName(descriptor.codec), normalized, Qt::CaseInsensitive) == 0 ||
            QString::compare(descriptor.label, normalized, Qt::CaseInsensitive) == 0) {
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

QString AudioFormatCatalog::findMatchingPreset(const AudioEncodingTarget& target) {
    if (target.sampleRate.policy != SampleRatePolicy::PreserveSource) {
        return QStringLiteral("custom");
    }
    for (const auto& preset : builtinPresets()) {
        if (preset.container != target.container || preset.codec != target.codec) {
            continue;
        }
        if (target.codec == AudioCodec::Mp3) {
            const auto* options = std::get_if<Mp3Options>(&target.codecOptions);
            if (options != nullptr && options->bitrateKbps == preset.bitrate &&
                options->bitrateMode == Mp3BitrateMode::Constant) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::Flac) {
            const auto* options = std::get_if<FlacOptions>(&target.codecOptions);
            if (options != nullptr && options->bitDepth == preset.bitDepth &&
                (preset.compressionLevel == -1 || options->compressionLevel == preset.compressionLevel)) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::Pcm) {
            const auto* options = std::get_if<PcmOptions>(&target.codecOptions);
            if (options != nullptr && options->sampleFormat == preset.sampleFormat) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::Vorbis) {
            const auto* options = std::get_if<VorbisOptions>(&target.codecOptions);
            if (options != nullptr && options->quality == preset.vorbisQuality) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::Aac) {
            const auto* options = std::get_if<AacOptions>(&target.codecOptions);
            if (options != nullptr && options->bitrateKbps == preset.bitrate) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::Alac) {
            const auto* options = std::get_if<AlacOptions>(&target.codecOptions);
            if (options != nullptr && options->bitDepth == preset.bitDepth) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::WavPack) {
            const auto* options = std::get_if<WavPackOptions>(&target.codecOptions);
            if (options != nullptr && options->bitDepth == preset.bitDepth) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::Opus) {
            const auto* options = std::get_if<OpusOptions>(&target.codecOptions);
            if (options != nullptr && options->bitrateKbps == preset.bitrate) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::Ac3) {
            const auto* options = std::get_if<Ac3Options>(&target.codecOptions);
            if (options != nullptr && options->bitrateKbps == preset.bitrate) {
                return preset.id;
            }
        } else if (target.codec == AudioCodec::Eac3) {
            const auto* options = std::get_if<Eac3Options>(&target.codecOptions);
            if (options != nullptr && options->bitrateKbps == preset.bitrate) {
                return preset.id;
            }
        }
    }
    return QStringLiteral("custom");
}

std::optional<AudioEncodingTarget> AudioFormatCatalog::targetFromPreset(const QString& presetId) {
    const QString normalized = presetId.trimmed().toLower();
    if (normalized == QStringLiteral("custom")) {
        return std::nullopt;
    }
    const auto descriptor = preset(normalized);
    if (!descriptor.has_value()) {
        return std::nullopt;
    }
    AudioEncodingTarget target = defaultEncodingTarget(descriptor->container, descriptor->codec);
    if (descriptor->codec == AudioCodec::Mp3 && descriptor->bitrate > 0) {
        target.codecOptions = Mp3Options{descriptor->bitrate, Mp3BitrateMode::Constant};
    } else if (descriptor->codec == AudioCodec::Flac && descriptor->bitDepth > 0) {
        target.codecOptions = FlacOptions{
            descriptor->bitDepth, descriptor->compressionLevel >= 0 ? descriptor->compressionLevel : 5};
    } else if (descriptor->codec == AudioCodec::Pcm && descriptor->sampleFormat != AudioSampleFormat::Unknown) {
        target.codecOptions = PcmOptions{descriptor->sampleFormat};
    } else if (descriptor->codec == AudioCodec::Vorbis && descriptor->vorbisQuality >= -1) {
        target.codecOptions = VorbisOptions{descriptor->vorbisQuality};
    } else if (descriptor->codec == AudioCodec::Aac && descriptor->bitrate > 0) {
        target.codecOptions = AacOptions{descriptor->bitrate};
    } else if (descriptor->codec == AudioCodec::Alac && descriptor->bitDepth > 0) {
        target.codecOptions = AlacOptions{descriptor->bitDepth};
    } else if (descriptor->codec == AudioCodec::WavPack && descriptor->bitDepth > 0) {
        target.codecOptions = WavPackOptions{descriptor->bitDepth};
    } else if (descriptor->codec == AudioCodec::Opus && descriptor->bitrate > 0) {
        target.codecOptions = OpusOptions{descriptor->bitrate};
    } else if (descriptor->codec == AudioCodec::Ac3 && descriptor->bitrate > 0) {
        target.codecOptions = Ac3Options{descriptor->bitrate};
    } else if (descriptor->codec == AudioCodec::Eac3 && descriptor->bitrate > 0) {
        target.codecOptions = Eac3Options{descriptor->bitrate};
    }
    return target;
}

QStringList AudioFormatCatalog::presetOptions() {
    QStringList options;
    for (const auto& descriptor : builtinPresets()) {
        options.append(descriptor.id);
    }
    options.append(QStringLiteral("custom"));
    return options;
}

QStringList AudioFormatCatalog::presetOptionLabels() {
    QStringList labels;
    for (const auto& descriptor : builtinPresets()) {
        labels.append(descriptor.label);
    }
    labels.append(QStringLiteral("Custom"));
    return labels;
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

QStringList AudioFormatCatalog::sampleRateOptionLabels(const AudioContainer container,
                                                        const AudioCodec codec) {
    const auto opts = sampleRateOptions(container, codec);
    QStringList labels;
    for (const QString& opt : opts) {
        if (opt == QStringLiteral("preserve")) {
            labels.append(QStringLiteral("Preserve Source"));
        } else {
            bool ok = false;
            const double rate = opt.toDouble(&ok);
            if (ok) {
                const auto numeric = static_cast<long long>(rate);
                labels.append(QStringLiteral("%1 kHz").arg(rate / 1000.0, 0, 'f', numeric % 1000 == 0 ? 0 : 1));
            } else {
                labels.append(opt);
            }
        }
    }
    return labels;
}

QStringList AudioFormatCatalog::bitrateOptionLabels(const AudioContainer container,
                                                     const AudioCodec codec) {
    const auto opts = bitrateOptions(container, codec);
    QStringList labels;
    for (const QString& opt : opts) {
        labels.append(opt + QStringLiteral(" kbps"));
    }
    return labels;
}

QStringList AudioFormatCatalog::bitDepthOptionLabels(const AudioContainer container,
                                                      const AudioCodec codec) {
    const auto opts = bitDepthOptions(container, codec);
    QStringList labels;
    for (const QString& opt : opts) {
        labels.append(opt + QStringLiteral("-bit"));
    }
    return labels;
}

QStringList AudioFormatCatalog::compressionLevelOptionLabels(const AudioContainer container,
                                                               const AudioCodec codec) {
    const auto opts = compressionLevelOptions(container, codec);
    QStringList labels;
    for (const QString& opt : opts) {
        labels.append(QStringLiteral("Level ") + opt);
    }
    return labels;
}

QStringList AudioFormatCatalog::sampleFormatOptionLabels(const AudioContainer container,
                                                          const AudioCodec codec) {
    const auto descriptor = target(container, codec);
    if (!descriptor.has_value()) {
        return {};
    }
    QStringList labels;
    for (const AudioSampleFormat fmt : descriptor->validSampleFormats) {
        switch (fmt) {
        case AudioSampleFormat::SignedInt16: labels.append(QStringLiteral("16-bit PCM")); break;
        case AudioSampleFormat::SignedInt24: labels.append(QStringLiteral("24-bit PCM")); break;
        case AudioSampleFormat::SignedInt32: labels.append(QStringLiteral("32-bit PCM")); break;
        case AudioSampleFormat::Float32: labels.append(QStringLiteral("32-bit Float")); break;
        case AudioSampleFormat::UnsignedInt8: labels.append(QStringLiteral("8-bit PCM")); break;
        case AudioSampleFormat::Float64: labels.append(QStringLiteral("64-bit Float")); break;
        case AudioSampleFormat::Unknown: labels.append(QStringLiteral("Unknown Format")); break;
        }
    }
    return labels;
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
