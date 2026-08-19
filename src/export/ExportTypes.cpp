#include "ExportTypes.hpp"

#include "AudioFormatCatalog.hpp"
#include "history/RecordingIndex.hpp"

namespace abracapture::exporting {

namespace {

bool setValidationError(QString* const error, const QString& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool isSupportedMp3Bitrate(const std::uint32_t bitrateKbps) {
    return bitrateKbps == 128 || bitrateKbps == 192 || bitrateKbps == 256 ||
           bitrateKbps == 320;
}

} // namespace

// ---------------------------------------------------------------------------
// Legacy ExportFormat interop
// ---------------------------------------------------------------------------

std::optional<ExportFormat> exportFormatFromString(const QString& format) {
    const QString normalized = format.trimmed().toLower();
    if (normalized == QStringLiteral("mp3")) return ExportFormat::Mp3;
    if (normalized == QStringLiteral("flac")) return ExportFormat::Flac;
    if (normalized == QStringLiteral("wav") || normalized == QStringLiteral("wave"))
        return ExportFormat::Wav;
    if (normalized == QStringLiteral("ogg") || normalized == QStringLiteral("vorbis"))
        return ExportFormat::OggVorbis;
    return std::nullopt;
}

QString exportFormatName(const ExportFormat format) {
    switch (format) {
    case ExportFormat::Mp3: return QStringLiteral("mp3");
    case ExportFormat::Flac: return QStringLiteral("flac");
    case ExportFormat::Wav: return QStringLiteral("wav");
    case ExportFormat::OggVorbis: return QStringLiteral("ogg");
    }
    return QStringLiteral("unknown");
}

QString exportFileExtension(const ExportFormat format) {
    return exportFormatName(format);
}

AudioContainer containerForLegacyFormat(const ExportFormat format) {
    switch (format) {
    case ExportFormat::Mp3: return AudioContainer::Mp3;
    case ExportFormat::Flac: return AudioContainer::Flac;
    case ExportFormat::Wav: return AudioContainer::Wav;
    case ExportFormat::OggVorbis: return AudioContainer::Ogg;
    }
    return AudioContainer::Mp3;
}

AudioCodec codecForLegacyFormat(const ExportFormat format) {
    switch (format) {
    case ExportFormat::Mp3: return AudioCodec::Mp3;
    case ExportFormat::Flac: return AudioCodec::Flac;
    case ExportFormat::Wav: return AudioCodec::Pcm;
    case ExportFormat::OggVorbis: return AudioCodec::Vorbis;
    }
    return AudioCodec::Mp3;
}

std::optional<ExportFormat> legacyFormatFor(const AudioContainer container,
                                             const AudioCodec codec) {
    if (container == AudioContainer::Mp3 && codec == AudioCodec::Mp3) return ExportFormat::Mp3;
    if (container == AudioContainer::Flac && codec == AudioCodec::Flac) return ExportFormat::Flac;
    if (container == AudioContainer::Wav && codec == AudioCodec::Pcm) return ExportFormat::Wav;
    if (container == AudioContainer::Ogg && codec == AudioCodec::Vorbis) return ExportFormat::OggVorbis;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// AudioEncodingTarget
// ---------------------------------------------------------------------------

bool AudioEncodingTarget::validate(QString* const error) const {
    const auto descriptor = AudioFormatCatalog::target(container, codec);
    if (!descriptor.has_value()) {
        return setValidationError(error, QStringLiteral("Invalid container/codec combination"));
    }

    if (sampleRate.policy == SampleRatePolicy::PreserveSource && sampleRate.sampleRate != 0) {
        return setValidationError(
            error, QStringLiteral("Preserve-source sample rate must not include an explicit rate"));
    }
    if (sampleRate.policy == SampleRatePolicy::Explicit &&
        !supportsExplicitSampleRate(container, codec, sampleRate.sampleRate)) {
        return setValidationError(
            error, QStringLiteral("Selected export sample rate is not supported by this encoder"));
    }

    switch (codec) {
    case AudioCodec::Mp3: {
        const auto* const mp3 = std::get_if<Mp3Options>(&codecOptions);
        if (mp3 == nullptr) {
            return setValidationError(error, QStringLiteral("MP3 export requires MP3 options"));
        }
        if (mp3->bitrateMode != Mp3BitrateMode::Constant) {
            return setValidationError(error, QStringLiteral("MP3 VBR export is not implemented"));
        }
        if (!descriptor->validBitrates.contains(mp3->bitrateKbps)) {
            return setValidationError(
                error, QStringLiteral("MP3 bitrate must be 128, 192, 256, or 320 kbps"));
        }
        return true;
    }
    case AudioCodec::Flac: {
        const auto* const flac = std::get_if<FlacOptions>(&codecOptions);
        if (flac == nullptr) {
            return setValidationError(error, QStringLiteral("FLAC export requires FLAC options"));
        }
        if (!descriptor->validBitDepths.contains(flac->bitDepth)) {
            return setValidationError(error, QStringLiteral("FLAC bit depth must be 16, 24, or 32"));
        }
        if (!descriptor->validCompressionLevels.contains(flac->compressionLevel)) {
            return setValidationError(error, QStringLiteral("FLAC compression level must be between 0 and 12"));
        }
        return true;
    }
    case AudioCodec::Pcm: {
        const auto* const pcm = std::get_if<PcmOptions>(&codecOptions);
        if (pcm == nullptr) {
            return setValidationError(error, QStringLiteral("WAV export requires PCM options"));
        }
        if (!descriptor->validSampleFormats.contains(pcm->sampleFormat)) {
            return setValidationError(error, QStringLiteral("Unsupported WAV sample format"));
        }
        return true;
    }
    case AudioCodec::Vorbis: {
        const auto* const vorbis = std::get_if<VorbisOptions>(&codecOptions);
        if (vorbis == nullptr) {
            return setValidationError(error, QStringLiteral("Ogg/Vorbis export requires Vorbis options"));
        }
        if (!descriptor->validVorbisQualities.contains(vorbis->quality)) {
            return setValidationError(error, QStringLiteral("Ogg/Vorbis quality must be between -1 and 10"));
        }
        return true;
    }
    }
    return setValidationError(error, QStringLiteral("Unsupported codec"));
}

QString AudioEncodingTarget::fileExtension() const {
    return AudioFormatCatalog::fileExtension(container, codec);
}

bool AudioEncodingTarget::isLossless() const noexcept {
    return isLosslessCodec(codec);
}

AudioEncodingTarget defaultEncodingTarget(const AudioContainer container, const AudioCodec codec) {
    AudioEncodingTarget target;
    target.container = container;
    target.codec = codec;
    target.sampleRate = {SampleRatePolicy::PreserveSource, 0};

    switch (codec) {
    case AudioCodec::Mp3:
        target.codecOptions = Mp3Options{320, Mp3BitrateMode::Constant};
        break;
    case AudioCodec::Flac:
        target.codecOptions = FlacOptions{16, 5};
        break;
    case AudioCodec::Pcm:
        target.codecOptions = PcmOptions{AudioSampleFormat::SignedInt16};
        break;
    case AudioCodec::Vorbis:
        target.codecOptions = VorbisOptions{5};
        break;
    }
    return target;
}

bool supportsExplicitSampleRate(const AudioContainer container, const AudioCodec codec,
                                 const std::uint32_t sampleRate) {
    return AudioFormatCatalog::supportsExplicitSampleRate(container, codec, sampleRate);
}

// ---------------------------------------------------------------------------
// ExportProfile
// ---------------------------------------------------------------------------

bool ExportProfile::validate(QString* const error) const {
    return target.validate(error);
}

ExportProfile defaultExportProfile(const ExportFormat format) {
    return defaultExportProfile(containerForLegacyFormat(format), codecForLegacyFormat(format));
}

ExportProfile defaultExportProfile(const AudioContainer container, const AudioCodec codec) {
    ExportProfile profile;
    profile.target = defaultEncodingTarget(container, codec);
    profile.collisionPolicy = CollisionPolicy::GenerateUniqueName;
    return profile;
}

// ---------------------------------------------------------------------------
// Legacy migration
// ---------------------------------------------------------------------------

ExportProfile migrateProfile(const LegacyExportProfile& legacy) {
    ExportProfile profile;
    profile.target.container = containerForLegacyFormat(legacy.format);
    profile.target.codec = codecForLegacyFormat(legacy.format);
    profile.target.sampleRate = legacy.sampleRate;
    profile.collisionPolicy = legacy.collisionPolicy;

    if (const auto* mp3 = std::get_if<Mp3Options>(&legacy.options)) {
        profile.target.codecOptions = *mp3;
    } else if (const auto* flac = std::get_if<FlacOptions>(&legacy.options)) {
        profile.target.codecOptions = *flac;
    } else if (const auto* pcm = std::get_if<PcmOptions>(&legacy.options)) {
        profile.target.codecOptions = *pcm;
    } else if (const auto* vorbis = std::get_if<VorbisOptions>(&legacy.options)) {
        profile.target.codecOptions = *vorbis;
    }

    return profile;
}

abracapture::history::DerivedExport createDerivedExportRecord(
    const ExportJobId jobId,
    const ExportRequest& request,
    const QString& state,
    const QString& outputPath,
    const QString& error) {
    abracapture::history::DerivedExport derived;
    derived.jobId = jobId;
    derived.container = audioContainerName(request.profile.target.container);
    derived.codec = audioCodecName(request.profile.target.codec);
    derived.format = request.profile.target.fileExtension();

    if (request.profile.target.sampleRate.policy == SampleRatePolicy::Explicit) {
        derived.sampleRate = request.profile.target.sampleRate.sampleRate;
    } else {
        derived.sampleRate = request.sourceFormat.sampleRate;
    }

    if (const auto* flac = std::get_if<FlacOptions>(&request.profile.target.codecOptions)) {
        derived.bitDepth = flac->bitDepth;
    } else if (const auto* pcm = std::get_if<PcmOptions>(&request.profile.target.codecOptions)) {
        derived.bitDepth = audioSampleFormatBits(pcm->sampleFormat);
    }

    if (const auto* mp3 = std::get_if<Mp3Options>(&request.profile.target.codecOptions)) {
        derived.bitrate = mp3->bitrateKbps;
    }

    derived.outputPath = outputPath.isEmpty()
                             ? QString::fromStdString(request.destinationPath.string())
                             : outputPath;
    derived.state = state;
    derived.error = error;
    return derived;
}

} // namespace abracapture::exporting
