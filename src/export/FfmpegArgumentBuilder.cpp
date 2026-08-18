#include "FfmpegArgumentBuilder.hpp"

namespace abracapture::exporting {

namespace {

QString pcmCodecName(const AudioSampleFormat sampleFormat) {
    switch (sampleFormat) {
    case AudioSampleFormat::UnsignedInt8: return QStringLiteral("pcm_u8");
    case AudioSampleFormat::SignedInt16: return QStringLiteral("pcm_s16le");
    case AudioSampleFormat::SignedInt24: return QStringLiteral("pcm_s24le");
    case AudioSampleFormat::SignedInt32: return QStringLiteral("pcm_s32le");
    case AudioSampleFormat::Float32: return QStringLiteral("pcm_f32le");
    case AudioSampleFormat::Unknown:
    case AudioSampleFormat::Float64:
        return QStringLiteral("pcm_s16le");
    }
    return QStringLiteral("pcm_s16le");
}

void appendFlacBitDepthArguments(QStringList& arguments, const std::uint32_t bitDepth) {
    if (bitDepth == 16) {
        arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s16");
        return;
    }
    arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s32")
              << QStringLiteral("-bits_per_raw_sample") << QString::number(bitDepth);
}

QString ditherFilter(const QString& sampleFormat, const int outputBits,
                     const SampleRateOption& sampleRate) {
    QString filter = QStringLiteral("aresample=out_sample_fmt=%1:output_sample_bits=%2:dither_method=triangular")
                         .arg(sampleFormat)
                         .arg(outputBits);
    if (sampleRate.policy == SampleRatePolicy::Explicit) {
        filter += QStringLiteral(":out_sample_rate=%1").arg(sampleRate.sampleRate);
    }
    return filter;
}

bool appendPcmDitherArguments(QStringList& arguments, const SampleRateOption& sampleRate,
                              const QString& sampleFormat, const int outputBits) {
    if (outputBits >= 32) {
        return false;
    }
    arguments << QStringLiteral("-af") << ditherFilter(sampleFormat, outputBits, sampleRate);
    return true;
}

bool supportsMp3PreserveSourceRate(const std::uint32_t sampleRate) {
    return sampleRate == 8000 || sampleRate == 11025 || sampleRate == 12000 ||
           sampleRate == 16000 || sampleRate == 22050 || sampleRate == 24000 ||
           sampleRate == 32000 || sampleRate == 44100 || sampleRate == 48000;
}

} // namespace

bool FfmpegArgumentBuilder::build(const ExportRequest& request,
                                  QStringList* const arguments,
                                  QString* const error) {
    if (arguments == nullptr || error == nullptr) {
        return false;
    }
    arguments->clear();
    error->clear();

    if (request.sourcePath.empty()) {
        *error = QStringLiteral("Export source path is empty");
        return false;
    }
    if (request.destinationPath.empty()) {
        *error = QStringLiteral("Export destination path is empty");
        return false;
    }
    if (!request.profile.validate(error)) {
        return false;
    }

    const auto& target = request.profile.target;

    if (target.codec == AudioCodec::Mp3 &&
        target.sampleRate.policy == SampleRatePolicy::PreserveSource &&
        request.sourceFormat.sampleRate != 0 &&
        !supportsMp3PreserveSourceRate(request.sourceFormat.sampleRate)) {
        *error = QStringLiteral("MP3 cannot preserve a %1 Hz source rate; choose 44.1 or 48 kHz")
                     .arg(request.sourceFormat.sampleRate);
        return false;
    }

    *arguments = {QStringLiteral("-nostdin"), QStringLiteral("-n"),
                  QStringLiteral("-i"),
                  QString::fromStdString(request.sourcePath.string())};
    bool sampleRateHandledByFilter = false;

    switch (target.codec) {
    case AudioCodec::Mp3: {
        const auto& options = std::get<Mp3Options>(target.codecOptions);
        *arguments << QStringLiteral("-c:a") << QStringLiteral("libmp3lame")
                   << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(options.bitrateKbps);
        break;
    }
    case AudioCodec::Flac: {
        const auto& options = std::get<FlacOptions>(target.codecOptions);
        sampleRateHandledByFilter = appendPcmDitherArguments(
            *arguments, target.sampleRate,
            options.bitDepth == 16 ? QStringLiteral("s16") : QStringLiteral("s32"),
            static_cast<int>(options.bitDepth));
        *arguments << QStringLiteral("-c:a") << QStringLiteral("flac")
                   << QStringLiteral("-compression_level")
                   << QString::number(options.compressionLevel);
        appendFlacBitDepthArguments(*arguments, options.bitDepth);
        break;
    }
    case AudioCodec::Pcm: {
        const auto& options = std::get<PcmOptions>(target.codecOptions);
        switch (options.sampleFormat) {
        case AudioSampleFormat::UnsignedInt8:
            sampleRateHandledByFilter = appendPcmDitherArguments(
                *arguments, target.sampleRate, QStringLiteral("u8"), 8);
            break;
        case AudioSampleFormat::SignedInt16:
            sampleRateHandledByFilter = appendPcmDitherArguments(
                *arguments, target.sampleRate, QStringLiteral("s16"), 16);
            break;
        case AudioSampleFormat::SignedInt24:
            sampleRateHandledByFilter = appendPcmDitherArguments(
                *arguments, target.sampleRate, QStringLiteral("s32"), 24);
            break;
        case AudioSampleFormat::SignedInt32:
        case AudioSampleFormat::Float32:
        case AudioSampleFormat::Float64:
        case AudioSampleFormat::Unknown:
            break;
        }
        *arguments << QStringLiteral("-c:a") << pcmCodecName(options.sampleFormat);
        break;
    }
    case AudioCodec::Vorbis: {
        const auto& options = std::get<VorbisOptions>(target.codecOptions);
        *arguments << QStringLiteral("-c:a") << QStringLiteral("libvorbis")
                   << QStringLiteral("-q:a") << QString::number(options.quality)
                   << QStringLiteral("-f") << QStringLiteral("ogg");
        break;
    }
    }

    if (target.sampleRate.policy == SampleRatePolicy::Explicit &&
        !sampleRateHandledByFilter) {
        *arguments << QStringLiteral("-ar")
                   << QString::number(target.sampleRate.sampleRate);
    }

    *arguments << QString::fromStdString(request.destinationPath.string());
    return true;
}

} // namespace abracapture::exporting
