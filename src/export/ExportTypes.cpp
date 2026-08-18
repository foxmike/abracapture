#include "ExportTypes.hpp"

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

bool isStandardExportSampleRate(const std::uint32_t sampleRate) {
    return sampleRate == 44100 || sampleRate == 48000 || sampleRate == 88200 ||
           sampleRate == 96000 || sampleRate == 176400 || sampleRate == 192000;
}

} // namespace

std::optional<ExportFormat> exportFormatFromString(const QString& format) {
    const QString normalized = format.trimmed().toLower();
    if (normalized == QStringLiteral("mp3")) {
        return ExportFormat::Mp3;
    }
    if (normalized == QStringLiteral("flac")) {
        return ExportFormat::Flac;
    }
    if (normalized == QStringLiteral("wav") || normalized == QStringLiteral("wave")) {
        return ExportFormat::Wav;
    }
    if (normalized == QStringLiteral("ogg") || normalized == QStringLiteral("vorbis")) {
        return ExportFormat::OggVorbis;
    }
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

bool supportsExplicitSampleRate(const ExportFormat format, const std::uint32_t sampleRate) {
    if (!isStandardExportSampleRate(sampleRate)) {
        return false;
    }
    switch (format) {
    case ExportFormat::Mp3:
        return sampleRate == 44100 || sampleRate == 48000;
    case ExportFormat::Flac:
    case ExportFormat::Wav:
        return true;
    case ExportFormat::OggVorbis:
        return sampleRate == 44100 || sampleRate == 48000;
    }
    return false;
}

bool ExportProfile::validate(QString* const error) const {
    if (sampleRate.policy == SampleRatePolicy::PreserveSource && sampleRate.sampleRate != 0) {
        return setValidationError(
            error, QStringLiteral("Preserve-source sample rate must not include an explicit rate"));
    }
    if (sampleRate.policy == SampleRatePolicy::Explicit &&
        !supportsExplicitSampleRate(format, sampleRate.sampleRate)) {
        return setValidationError(
            error, QStringLiteral("Selected export sample rate is not supported by this encoder"));
    }

    switch (format) {
    case ExportFormat::Mp3: {
        const auto* const mp3 = std::get_if<Mp3Options>(&options);
        if (mp3 == nullptr) {
            return setValidationError(error, QStringLiteral("MP3 export requires MP3 options"));
        }
        if (mp3->bitrateMode != Mp3BitrateMode::Constant) {
            return setValidationError(error, QStringLiteral("MP3 VBR export is not implemented"));
        }
        if (!isSupportedMp3Bitrate(mp3->bitrateKbps)) {
            return setValidationError(
                error, QStringLiteral("MP3 bitrate must be 128, 192, 256, or 320 kbps"));
        }
        return true;
    }
    case ExportFormat::Flac: {
        const auto* const flac = std::get_if<FlacOptions>(&options);
        if (flac == nullptr) {
            return setValidationError(error, QStringLiteral("FLAC export requires FLAC options"));
        }
        if (flac->bitDepth != 16 && flac->bitDepth != 24 && flac->bitDepth != 32) {
            return setValidationError(error, QStringLiteral("FLAC bit depth must be 16, 24, or 32"));
        }
        if (flac->compressionLevel < 0 || flac->compressionLevel > 12) {
            return setValidationError(error, QStringLiteral("FLAC compression level must be between 0 and 12"));
        }
        return true;
    }
    case ExportFormat::Wav:
        if (!std::holds_alternative<WavOptions>(options)) {
            return setValidationError(error, QStringLiteral("WAV export requires WAV options"));
        }
        return true;
    case ExportFormat::OggVorbis: {
        const auto* const ogg = std::get_if<OggVorbisOptions>(&options);
        if (ogg == nullptr) {
            return setValidationError(error, QStringLiteral("Ogg/Vorbis export requires Ogg/Vorbis options"));
        }
        if (ogg->quality < -1 || ogg->quality > 10) {
            return setValidationError(error, QStringLiteral("Ogg/Vorbis quality must be between -1 and 10"));
        }
        return true;
    }
    }
    return setValidationError(error, QStringLiteral("Unsupported export format"));
}

ExportProfile defaultExportProfile(const ExportFormat format) {
    ExportProfile profile;
    profile.format = format;
    profile.sampleRate = {SampleRatePolicy::PreserveSource, 0};
    profile.collisionPolicy = CollisionPolicy::GenerateUniqueName;

    switch (format) {
    case ExportFormat::Mp3:
        profile.options = Mp3Options{320, Mp3BitrateMode::Constant};
        break;
    case ExportFormat::Flac:
        profile.options = FlacOptions{16, 5};
        break;
    case ExportFormat::Wav:
        profile.options = WavOptions{WavSampleFormat::Signed16};
        break;
    case ExportFormat::OggVorbis:
        profile.options = OggVorbisOptions{5};
        break;
    }
    return profile;
}

} // namespace abracapture::exporting
