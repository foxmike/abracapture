#include "FfmpegArgumentBuilder.hpp"

#include "Eac3Capability.hpp"

namespace abracapture::exporting {

namespace {

struct Eac3LayoutArgs {
    QString channelLayoutArgument;
    QString acArgument;
};

// Convert the source layout into a precise ffmpeg -channel_layout argument
// when we have explicit positions. We rely solely on the layout's own
// `ffmpegLayoutName()` to avoid hardcoding a "7.1" string. This guarantees
// the produced master is identified by the same layout names FFmpeg uses
// internally for its E-AC-3 encoder.
QString ffmpegLayoutArgument(const abracapture::exporting::AudioChannelLayout& layout) {
    if (!layout.hasExplicitPositions()) {
        return {};
    }
    const QString name = layout.ffmpegLayoutName();
    if (name.isEmpty()) {
        return {};
    }
    return name;
}

// Validate the requested E-AC-3 layout against the probed capability and
// decide which -channel_layout / -ac / -ar arguments must be emitted. A
// capability that does not honor the requested layout produces an honest
// error rather than a silent downgrade.
bool prepareEac3LayoutArgs(const ExportRequest& request,
                           const Eac3CapabilityReport& capability,
                           Eac3LayoutArgs* const layoutArgs,
                           QString* const error) {
    const auto& layout = request.sourceFormat.channelLayout;
    if (layout.is7Point1()) {
        if (!capability.supports71()) {
            *error = capability.unsupportedReason().isEmpty()
                         ? QStringLiteral("E-AC-3 7.1 export requested but the "
                                          "active FFmpeg encoder does not advertise "
                                          "8-channel E-AC-3 encoding.")
                         : capability.unsupportedReason();
            return false;
        }
        if (capability.status == Eac3CapabilityReport::Status::SupportedBySystemFfmpeg) {
            layoutArgs->channelLayoutArgument = QStringLiteral("7.1");
            layoutArgs->acArgument = QStringLiteral("8");
        }
        return true;
    }
    // For 5.1, FFmpeg's native encoder accepts the layout. We always emit the
    // explicit channel-layout arg so the encoder never has to guess from the
    // input WAV side.
    if (layout.is5Point1()) {
        layoutArgs->channelLayoutArgument = ffmpegLayoutArgument(layout);
        layoutArgs->acArgument = QStringLiteral("6");
        return true;
    }
    if (layout.hasExplicitPositions()) {
        layoutArgs->channelLayoutArgument = ffmpegLayoutArgument(layout);
        const auto count = layout.channelCount();
        if (count > 0) {
            layoutArgs->acArgument = QString::number(count);
        }
    }
    return true;
}

QString pcmCodecName(const AudioContainer container, const AudioSampleFormat sampleFormat) {
    if (container == AudioContainer::Aiff) {
        switch (sampleFormat) {
        case AudioSampleFormat::UnsignedInt8: return QStringLiteral("pcm_s8");
        case AudioSampleFormat::SignedInt16: return QStringLiteral("pcm_s16be");
        case AudioSampleFormat::SignedInt24: return QStringLiteral("pcm_s24be");
        case AudioSampleFormat::SignedInt32: return QStringLiteral("pcm_s32be");
        case AudioSampleFormat::Float32: return QStringLiteral("pcm_f32be");
        case AudioSampleFormat::Float64: return QStringLiteral("pcm_f64be");
        case AudioSampleFormat::Unknown: return QStringLiteral("pcm_s24be");
        }
        return QStringLiteral("pcm_s24be");
    }

    switch (sampleFormat) {
    case AudioSampleFormat::UnsignedInt8: return QStringLiteral("pcm_u8");
    case AudioSampleFormat::SignedInt16: return QStringLiteral("pcm_s16le");
    case AudioSampleFormat::SignedInt24: return QStringLiteral("pcm_s24le");
    case AudioSampleFormat::SignedInt32: return QStringLiteral("pcm_s32le");
    case AudioSampleFormat::Float32: return QStringLiteral("pcm_f32le");
    case AudioSampleFormat::Float64: return QStringLiteral("pcm_f64le");
    case AudioSampleFormat::Unknown: return QStringLiteral("pcm_s16le");
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

bool supportsAc3SampleRate(const std::uint32_t sampleRate) {
    return sampleRate == 32000 || sampleRate == 44100 || sampleRate == 48000;
}

} // namespace

bool FfmpegArgumentBuilder::build(const ExportRequest& request,
                                  QStringList* const arguments,
                                  QString* const error) {
    // Keep this compatibility overload nonblocking. Callers that need
    // E-AC-3 7.1 use the cached report owned by FfmpegCapabilities.
    return build(request, Eac3CapabilityReport::unsupportedDefault(), arguments, error);
}

bool FfmpegArgumentBuilder::build(const ExportRequest& request,
                                  const Eac3CapabilityReport& eac3Capability,
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
        *arguments << QStringLiteral("-c:a") << pcmCodecName(target.container, options.sampleFormat);
        if (target.container == AudioContainer::Aiff) {
            *arguments << QStringLiteral("-f") << QStringLiteral("aiff");
        } else if (target.container == AudioContainer::Caf) {
            *arguments << QStringLiteral("-f") << QStringLiteral("caf");
        } else if (target.container == AudioContainer::Wav) {
            *arguments << QStringLiteral("-f") << QStringLiteral("wav");
        }
        break;
    }
    case AudioCodec::Vorbis: {
        const auto& options = std::get<VorbisOptions>(target.codecOptions);
        *arguments << QStringLiteral("-c:a") << QStringLiteral("libvorbis")
                   << QStringLiteral("-q:a") << QString::number(options.quality)
                   << QStringLiteral("-f") << QStringLiteral("ogg");
        break;
    }
    case AudioCodec::Aac: {
        const auto& options = std::get<AacOptions>(target.codecOptions);
        *arguments << QStringLiteral("-c:a") << QStringLiteral("aac")
                   << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(options.bitrateKbps)
                   << QStringLiteral("-f") << QStringLiteral("ipod");
        break;
    }
    case AudioCodec::Alac: {
        const auto& options = std::get<AlacOptions>(target.codecOptions);
        sampleRateHandledByFilter = appendPcmDitherArguments(
            *arguments, target.sampleRate,
            options.bitDepth == 16 ? QStringLiteral("s16p") : QStringLiteral("s32p"),
            static_cast<int>(options.bitDepth));
        *arguments << QStringLiteral("-c:a") << QStringLiteral("alac");
        if (options.bitDepth == 16) {
            *arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s16p");
        } else {
            *arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s32p")
                       << QStringLiteral("-bits_per_raw_sample") << QString::number(options.bitDepth);
        }
        *arguments << QStringLiteral("-f") << QStringLiteral("ipod");
        break;
    }
    case AudioCodec::WavPack: {
        const auto& options = std::get<WavPackOptions>(target.codecOptions);
        sampleRateHandledByFilter = appendPcmDitherArguments(
            *arguments, target.sampleRate,
            options.bitDepth == 16 ? QStringLiteral("s16p") : QStringLiteral("s32p"),
            static_cast<int>(options.bitDepth));
        *arguments << QStringLiteral("-c:a") << QStringLiteral("wavpack");
        if (options.bitDepth == 16) {
            *arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s16p");
        } else if (options.bitDepth == 24) {
            *arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s32p")
                       << QStringLiteral("-bits_per_raw_sample") << QString::number(options.bitDepth);
        } else {
            *arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s32p");
        }
        *arguments << QStringLiteral("-f") << QStringLiteral("wv");
        break;
    }
    case AudioCodec::Opus: {
        const auto& options = std::get<OpusOptions>(target.codecOptions);
        *arguments << QStringLiteral("-c:a") << QStringLiteral("libopus")
                   << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(options.bitrateKbps)
                   << QStringLiteral("-f") << QStringLiteral("opus");
        if (target.sampleRate.policy == SampleRatePolicy::PreserveSource &&
            request.sourceFormat.sampleRate != 0 &&
            request.sourceFormat.sampleRate != 48000 &&
            request.sourceFormat.sampleRate != 24000 &&
            request.sourceFormat.sampleRate != 16000 &&
            request.sourceFormat.sampleRate != 12000 &&
            request.sourceFormat.sampleRate != 8000) {
            *arguments << QStringLiteral("-ar") << QStringLiteral("48000");
            sampleRateHandledByFilter = true;
        }
        break;
    }
    case AudioCodec::Ac3: {
        const auto& options = std::get<Ac3Options>(target.codecOptions);
        if (request.sourceFormat.channelLayout.channelCount() > 2 &&
            !request.sourceFormat.channelLayout.hasExplicitPositions()) {
            *error = QStringLiteral("AC-3 surround export requires known speaker positions; source layout is unknown");
            return false;
        }
        if (request.sourceFormat.channelLayout.is7Point1()) {
            *error = QStringLiteral("AC-3 does not support 7.1 surround; choose 5.1 or E-AC-3");
            return false;
        }
        if (target.sampleRate.policy == SampleRatePolicy::PreserveSource &&
            request.sourceFormat.sampleRate != 0 &&
            !supportsAc3SampleRate(request.sourceFormat.sampleRate)) {
            *error = QStringLiteral("AC-3 requires a sample rate of 32, 44.1, or 48 kHz");
            return false;
        }
        *arguments << QStringLiteral("-c:a") << QStringLiteral("ac3")
                   << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(options.bitrateKbps)
                   << QStringLiteral("-f") << QStringLiteral("ac3");
        break;
    }
    case AudioCodec::Eac3: {
        const auto& options = std::get<Eac3Options>(target.codecOptions);
        const auto& sourceLayout = request.sourceFormat.channelLayout;
        if (sourceLayout.channelCount() > 2 &&
            !sourceLayout.hasExplicitPositions()) {
            *error = QStringLiteral("E-AC-3 surround export requires known speaker positions; source layout is unknown");
            return false;
        }
        if (target.sampleRate.policy == SampleRatePolicy::PreserveSource &&
            request.sourceFormat.sampleRate != 0 &&
            !supportsAc3SampleRate(request.sourceFormat.sampleRate)) {
            *error = QStringLiteral("E-AC-3 requires a sample rate of 32, 44.1, or 48 kHz");
            return false;
        }
        Eac3LayoutArgs eac3Args;
        if (!prepareEac3LayoutArgs(request, eac3Capability, &eac3Args, error)) {
            return false;
        }
        *arguments << QStringLiteral("-c:a") << QStringLiteral("eac3")
                   << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(options.bitrateKbps);
        if (!eac3Args.channelLayoutArgument.isEmpty()) {
            *arguments << QStringLiteral("-channel_layout") << eac3Args.channelLayoutArgument;
        }
        if (!eac3Args.acArgument.isEmpty()) {
            *arguments << QStringLiteral("-ac") << eac3Args.acArgument;
        }
        *arguments << QStringLiteral("-f") << QStringLiteral("eac3");
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
