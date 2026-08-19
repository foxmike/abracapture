#include "export/AudioDomainTypes.hpp"
#include "export/AudioFormatCatalog.hpp"
#include "export/ExportOutputPlanner.hpp"
#include "export/ExportManager.hpp"
#include "export/FfmpegCapabilities.hpp"
#include "export/ManualExportPlanner.hpp"
#include "export/FfmpegArgumentBuilder.hpp"
#include "export/ExportTypes.hpp"
#include "export/PathSanitizer.hpp"
#include "history/RecordingIndex.hpp"
#include "recording/OutputPathPolicy.hpp"
#include "recording/RecordingSink.hpp"
#include "ui/ManualExportViewModel.hpp"
#include "ui/SettingsViewModel.hpp"

#include <QCoreApplication>
#include <QDataStream>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

#include <sndfile.h>

#include <iostream>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const char* const message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        ++failures;
    }
}

bool writeMultichannelSilenceWav(const std::filesystem::path& path, int channels, int sampleRate,
                                 const QList<abracapture::exporting::AudioChannelPosition>& positions = {}) {
    SF_INFO info{};
    info.samplerate = sampleRate;
    info.channels = channels;
    info.format = SF_FORMAT_WAVEX | SF_FORMAT_PCM_24;
    SNDFILE* const file = sf_open(path.string().c_str(), SFM_WRITE, &info);
    if (!file) return false;
    if (!positions.isEmpty() && positions.size() == channels) {
        std::vector<int> channelMap;
        for (const auto pos : positions) {
            switch (pos) {
            case abracapture::exporting::AudioChannelPosition::FrontLeft: channelMap.push_back(SF_CHANNEL_MAP_FRONT_LEFT); break;
            case abracapture::exporting::AudioChannelPosition::FrontRight: channelMap.push_back(SF_CHANNEL_MAP_FRONT_RIGHT); break;
            case abracapture::exporting::AudioChannelPosition::FrontCenter: channelMap.push_back(SF_CHANNEL_MAP_FRONT_CENTER); break;
            case abracapture::exporting::AudioChannelPosition::Lfe: channelMap.push_back(SF_CHANNEL_MAP_LFE); break;
            case abracapture::exporting::AudioChannelPosition::RearLeft: channelMap.push_back(SF_CHANNEL_MAP_REAR_LEFT); break;
            case abracapture::exporting::AudioChannelPosition::RearRight: channelMap.push_back(SF_CHANNEL_MAP_REAR_RIGHT); break;
            case abracapture::exporting::AudioChannelPosition::SideLeft: channelMap.push_back(SF_CHANNEL_MAP_SIDE_LEFT); break;
            case abracapture::exporting::AudioChannelPosition::SideRight: channelMap.push_back(SF_CHANNEL_MAP_SIDE_RIGHT); break;
            }
        }
        if (static_cast<int>(channelMap.size()) == channels) {
            const int setResult = sf_command(file, SFC_SET_CHANNEL_MAP_INFO, channelMap.data(),
                                             static_cast<int>(sizeof(int) * channelMap.size()));
            static_cast<void>(setResult);
        }
    }
    std::vector<int> silence(static_cast<std::size_t>(channels * sampleRate / 10), 0); // 0.1s
    sf_write_int(file, silence.data(), static_cast<sf_count_t>(silence.size()));
    sf_close(file);
    return true;
}

abracapture::exporting::ExportRequest requestFor(
    const abracapture::exporting::ExportProfile& profile,
    const std::filesystem::path& destination = "/exports/track.mp3") {
    abracapture::exporting::ExportRequest request;
    request.sourcePath = "/masters/track.wav";
    request.destinationPath = destination;
    request.profile = profile;
    return request;
}

bool build(const abracapture::exporting::ExportRequest& request, QStringList* arguments) {
    QString error;
    const bool success = abracapture::exporting::FfmpegArgumentBuilder::build(
        request, arguments, &error);
    if (!success) {
        std::cerr << "Argument builder error: " << error.toStdString() << "\n";
    }
    return success;
}

bool writeSilenceWav(const std::filesystem::path& path) {
    constexpr quint32 sampleRate = 44100;
    constexpr quint16 channels = 1;
    constexpr quint16 bitsPerSample = 16;
    constexpr quint32 dataSize = sampleRate * channels * (bitsPerSample / 8);

    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + dataSize);
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32(16) << quint16(1) << channels << sampleRate;
    stream << quint32(sampleRate * channels * (bitsPerSample / 8));
    stream << quint16(channels * (bitsPerSample / 8)) << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataSize;
    stream.writeRawData(QByteArray(dataSize, '\0').constData(), dataSize);
    return stream.status() == QDataStream::Ok;
}

// ---- Part 1–3: AudioDomainTypes ----

void testAudioContainerRoundTrip() {
    for (const auto container : {abracapture::exporting::AudioContainer::Wav,
                                  abracapture::exporting::AudioContainer::Flac,
                                  abracapture::exporting::AudioContainer::Ogg,
                                  abracapture::exporting::AudioContainer::Mp3,
                                  abracapture::exporting::AudioContainer::Aiff,
                                  abracapture::exporting::AudioContainer::M4a,
                                  abracapture::exporting::AudioContainer::WavPack,
                                  abracapture::exporting::AudioContainer::Caf,
                                  abracapture::exporting::AudioContainer::Opus,
                                  abracapture::exporting::AudioContainer::Ac3,
                                  abracapture::exporting::AudioContainer::Eac3}) {
        const auto name = abracapture::exporting::audioContainerName(container);
        const auto parsed = abracapture::exporting::audioContainerFromString(name);
        expect(parsed.has_value() && *parsed == container,
               "AudioContainer round-trips through name/fromString");
    }
    expect(!abracapture::exporting::audioContainerFromString(QStringLiteral("invalid_container")).has_value(),
           "Unknown container string returns nullopt");
}

void testAudioCodecRoundTrip() {
    for (const auto codec : {abracapture::exporting::AudioCodec::Pcm,
                              abracapture::exporting::AudioCodec::Flac,
                              abracapture::exporting::AudioCodec::Mp3,
                              abracapture::exporting::AudioCodec::Vorbis,
                              abracapture::exporting::AudioCodec::Alac,
                              abracapture::exporting::AudioCodec::Aac,
                              abracapture::exporting::AudioCodec::WavPack,
                              abracapture::exporting::AudioCodec::Opus,
                              abracapture::exporting::AudioCodec::Ac3,
                              abracapture::exporting::AudioCodec::Eac3}) {
        const auto name = abracapture::exporting::audioCodecName(codec);
        const auto parsed = abracapture::exporting::audioCodecFromString(name);
        expect(parsed.has_value() && *parsed == codec,
               "AudioCodec round-trips through name/fromString");
    }
    expect(abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::Pcm),
           "PCM is lossless");
    expect(abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::Flac),
           "FLAC is lossless");
    expect(abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::Alac),
           "ALAC is lossless");
    expect(abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::WavPack),
           "WavPack is lossless");
    expect(!abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::Mp3),
           "MP3 is lossy");
    expect(!abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::Aac),
           "AAC is lossy");
    expect(!abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::Opus),
           "Opus is lossy");
    expect(!abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::Ac3),
           "AC-3 is lossy");
    expect(!abracapture::exporting::isLosslessCodec(abracapture::exporting::AudioCodec::Eac3),
           "E-AC-3 is lossy");
}

void testAudioSampleFormatRoundTrip() {
    for (const auto fmt : {abracapture::exporting::AudioSampleFormat::SignedInt16,
                            abracapture::exporting::AudioSampleFormat::SignedInt24,
                            abracapture::exporting::AudioSampleFormat::SignedInt32,
                            abracapture::exporting::AudioSampleFormat::Float32}) {
        const auto name = abracapture::exporting::audioSampleFormatName(fmt);
        const auto parsed = abracapture::exporting::audioSampleFormatFromString(name);
        expect(parsed.has_value() && *parsed == fmt,
               "AudioSampleFormat round-trips through name/fromString");
    }
    expect(abracapture::exporting::audioSampleFormatBits(
               abracapture::exporting::AudioSampleFormat::SignedInt24) == 24,
           "24-bit sample format reports 24 bits");
}

void testAudioChannelLayout() {
    const auto mono = abracapture::exporting::AudioChannelLayout::mono();
    expect(mono.channelCount() == 1 && mono.hasExplicitPositions() && mono.isMono(),
           "Mono layout has 1 channel with explicit position");
    expect(mono.displayName() == QStringLiteral("Mono"), "Mono displayName is 'Mono'");
    expect(mono.ffmpegLayoutName() == QStringLiteral("mono"), "Mono ffmpegLayoutName is 'mono'");

    const auto stereo = abracapture::exporting::AudioChannelLayout::stereo();
    expect(stereo.channelCount() == 2 && stereo.hasExplicitPositions() && stereo.isStereo(),
           "Stereo layout has 2 channels with explicit positions");
    expect(stereo.displayName() == QStringLiteral("Stereo"), "Stereo displayName is 'Stereo'");
    expect(stereo.ffmpegLayoutName() == QStringLiteral("stereo"), "Stereo ffmpegLayoutName is 'stereo'");

    const auto surround51 = abracapture::exporting::AudioChannelLayout::surround51();
    expect(surround51.channelCount() == 6 && surround51.hasExplicitPositions() && surround51.is5Point1(),
           "5.1 surround layout has 6 channels with explicit positions");
    expect(surround51.displayName() == QStringLiteral("5.1"), "5.1 displayName is '5.1'");
    expect(surround51.ffmpegLayoutName() == QStringLiteral("5.1"), "5.1 ffmpegLayoutName is '5.1'");

    const auto surround71 = abracapture::exporting::AudioChannelLayout::surround71();
    expect(surround71.channelCount() == 8 && surround71.hasExplicitPositions() && surround71.is7Point1(),
           "7.1 surround layout has 8 channels with explicit positions");
    expect(surround71.displayName() == QStringLiteral("7.1"), "7.1 displayName is '7.1'");
    expect(surround71.ffmpegLayoutName() == QStringLiteral("7.1"), "7.1 ffmpegLayoutName is '7.1'");

    const auto unknown6 = abracapture::exporting::AudioChannelLayout::unknown(6);
    expect(unknown6.channelCount() == 6 && !unknown6.hasExplicitPositions() && !unknown6.is5Point1(),
           "Unknown(6) has 6 channels without inferred positions");
    expect(unknown6.displayName() == QStringLiteral("6 channels — layout unknown"),
           "Unknown(6) displayName is '6 channels — layout unknown'");
    expect(unknown6.ffmpegLayoutName().isEmpty(), "Unknown(6) ffmpegLayoutName is empty");

    const auto fromNames = abracapture::exporting::AudioChannelLayout::fromPositionNames(
        {QStringLiteral("FL"), QStringLiteral("FR"), QStringLiteral("FC"),
         QStringLiteral("LFE"), QStringLiteral("RL"), QStringLiteral("RR")});
    expect(fromNames.is5Point1(), "fromPositionNames reconstructs 5.1 layout");
}

void testAudioStreamFormat() {
    abracapture::exporting::AudioStreamFormat fmt;
    fmt.sampleRate = 48000;
    fmt.sampleFormat = abracapture::exporting::AudioSampleFormat::Float32;
    fmt.channelLayout = abracapture::exporting::AudioChannelLayout::stereo();
    expect(fmt.sampleRate == 48000 && fmt.channelLayout.channelCount() == 2,
           "AudioStreamFormat holds rate, format, and layout");
    abracapture::exporting::AudioStreamFormat same = fmt;
    expect(same == fmt, "AudioStreamFormat equality works");
}

// ---- Part 2: AudioEncodingTarget ----

void testAudioEncodingTargetValidation() {
    auto target = abracapture::exporting::defaultEncodingTarget(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    QString error;
    expect(target.validate(&error), "Default MP3 target is valid");
    expect(target.fileExtension() == QStringLiteral("mp3"), "MP3 target extension is mp3");
    expect(!target.isLossless(), "MP3 target is not lossless");

    auto flacTarget = abracapture::exporting::defaultEncodingTarget(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
    expect(flacTarget.validate(&error), "Default FLAC target is valid");
    expect(flacTarget.isLossless(), "FLAC target is lossless");

    auto mismatch = target;
    mismatch.codecOptions = abracapture::exporting::FlacOptions{};
    expect(!mismatch.validate(&error), "Mismatched codec options fail validation");
}

// ---- Part 4: AudioFormatCatalog ----

void testAudioFormatCatalog() {
    const auto& targets = abracapture::exporting::AudioFormatCatalog::targets();
    expect(targets.size() >= 4, "Catalog has at least 4 targets (MP3, FLAC, WAV, Ogg Vorbis)");
    expect(abracapture::exporting::AudioFormatCatalog::isValidPairing(
               abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3),
           "MP3/MP3 is a valid pairing");
    expect(!abracapture::exporting::AudioFormatCatalog::isValidPairing(
               abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Flac),
           "MP3/FLAC is not a valid pairing");

    const auto mp3Bitrates = abracapture::exporting::AudioFormatCatalog::bitrateOptions(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    expect(mp3Bitrates.contains(QStringLiteral("320")), "MP3 bitrate options include 320");

    const auto flacDepths = abracapture::exporting::AudioFormatCatalog::bitDepthOptions(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
    expect(flacDepths.contains(QStringLiteral("24")), "FLAC bit depth options include 24");

    const auto wavFormats = abracapture::exporting::AudioFormatCatalog::sampleFormatOptions(
        abracapture::exporting::AudioContainer::Wav, abracapture::exporting::AudioCodec::Pcm);
    expect(!wavFormats.isEmpty(), "WAV sample format options are non-empty");

    const auto& presets = abracapture::exporting::AudioFormatCatalog::presets();
    expect(presets.size() >= 6, "Catalog has at least 6 presets");
    const auto mp3320 = abracapture::exporting::AudioFormatCatalog::preset(QStringLiteral("mp3_320"));
    expect(mp3320.has_value() && mp3320->bitrate == 320, "mp3_320 preset has 320 bitrate");

    const auto foundTarget = abracapture::exporting::AudioFormatCatalog::findTarget(QStringLiteral("MP3"));
    expect(foundTarget.has_value() && foundTarget->container == abracapture::exporting::AudioContainer::Mp3,
           "findTarget resolves MP3 format string");
    expect(!abracapture::exporting::AudioFormatCatalog::findTarget(QStringLiteral("m4a")).has_value(),
           "findTarget rejects ambiguous M4A container-only lookup");

    const auto presetTarget = abracapture::exporting::AudioFormatCatalog::targetFromPreset(QStringLiteral("mp3_320"));
    expect(presetTarget.has_value() && presetTarget->container == abracapture::exporting::AudioContainer::Mp3,
           "targetFromPreset instantiates target from mp3_320 preset");
    expect(abracapture::exporting::AudioFormatCatalog::findMatchingPreset(*presetTarget) == QStringLiteral("mp3_320"),
           "findMatchingPreset recognizes target matching mp3_320");
}

// ---- Part 5: FfmpegCapabilities ----

void testFfmpegCapabilities() {
    abracapture::exporting::FfmpegCapabilities capabilities;
    static_cast<void>(capabilities.probe());
    if (!capabilities.isProbed()) {
        std::cerr << "  (skipping FfmpegCapabilities tests — ffmpeg not found)\n";
        return;
    }
    expect(!capabilities.ffmpegPath().isEmpty(), "FfmpegCapabilities finds ffmpeg path");
    expect(!capabilities.version().isEmpty(), "FfmpegCapabilities reads version");
    expect(capabilities.isEncoderAvailable(QStringLiteral("libmp3lame")),
           "libmp3lame encoder is available");
    const auto mp3Encoder = capabilities.encoderForCodec(abracapture::exporting::AudioCodec::Mp3);
    expect(mp3Encoder == QStringLiteral("libmp3lame"), "MP3 codec maps to libmp3lame");
    const auto flacMuxer = capabilities.muxerForContainer(abracapture::exporting::AudioContainer::Flac);
    expect(!flacMuxer.isEmpty(), "FLAC container maps to a muxer");

    // E-AC-3 7.1 capability probe must return a structured report that
    // never claims arbitrary support. We assert the report's status is
    // one of the documented enum values, that its `supports71()` reflects
    // the system FFmpeg's actual `Supported channel layouts` listing, and
    // that `unsupportedReason()` is non-empty when 7.1 isn't available.
    const auto& eac3 = capabilities.eac3Capability();
    expect(eac3.statusName() != QStringLiteral("Unknown"),
           "E-AC-3 capability probe status is one of the documented enum values");
    if (!eac3.supports71()) {
        expect(!eac3.unsupportedReason().isEmpty(),
               "E-AC-3 capability probe produces an explicit reason when 7.1 is unavailable");
        expect(eac3.ffmpegMaxChannels <= 8,
               "E-AC-3 capability probe only reports max-channels it actually saw");
    } else {
        expect(eac3.ffmpegMaxChannels >= 8,
               "E-AC-3 capability probe reports at least 8 max-channels when 7.1 is supported");
        expect(abracapture::exporting::Eac3Capability::layoutSupports71(eac3.ffmpegSupportedLayouts),
               "E-AC-3 capability probe's supports71() matches the parsed layout list");
    }
}

// ---- Part 6: PathSanitizer ----

void testPathSanitizer() {
    const auto clean = abracapture::exporting::sanitizePathComponent("Hello World");
    expect(clean == "Hello World", "Clean string passes through unchanged");
    const auto traversal = abracapture::exporting::sanitizePathComponent("../../../etc/passwd");
    expect(traversal.find('/') == std::string::npos && traversal.find('\\') == std::string::npos,
           "Traversal paths have separators removed");
    const auto control = abracapture::exporting::sanitizePathComponent("track\x01\x02\x03name");
    expect(control.find('\x01') == std::string::npos, "Control characters are replaced");
    const auto dotdot = abracapture::exporting::sanitizePathComponent("..");
    expect(dotdot != "..", "Bare .. is neutralized");
    const auto reserved = abracapture::exporting::sanitizePathComponent("a:b*c?d<e>f|g");
    expect(reserved.find(':') == std::string::npos && reserved.find('*') == std::string::npos,
           "Reserved characters are replaced");

    std::string longName(200, 'a');
    const auto truncated = abracapture::exporting::sanitizePathComponent(longName);
    expect(truncated.size() <= 120, "Long names are truncated to 120 bytes");
}

// ---- Part 7: Legacy migration ----

void testLegacyFormatConversion() {
    expect(abracapture::exporting::containerForLegacyFormat(abracapture::exporting::ExportFormat::Mp3) ==
               abracapture::exporting::AudioContainer::Mp3,
           "ExportFormat::Mp3 maps to AudioContainer::Mp3");
    expect(abracapture::exporting::codecForLegacyFormat(abracapture::exporting::ExportFormat::OggVorbis) ==
               abracapture::exporting::AudioCodec::Vorbis,
           "ExportFormat::OggVorbis maps to AudioCodec::Vorbis");
    const auto roundTrip = abracapture::exporting::legacyFormatFor(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
    expect(roundTrip.has_value() && *roundTrip == abracapture::exporting::ExportFormat::Flac,
           "Container/codec round-trips to legacy format");

    abracapture::exporting::LegacyExportProfile legacy;
    legacy.format = abracapture::exporting::ExportFormat::Mp3;
    legacy.options = abracapture::exporting::Mp3Options{256, abracapture::exporting::Mp3BitrateMode::Constant};
    const auto migrated = abracapture::exporting::migrateProfile(legacy);
    expect(migrated.target.container == abracapture::exporting::AudioContainer::Mp3 &&
               migrated.target.codec == abracapture::exporting::AudioCodec::Mp3,
           "Migrated profile has correct container/codec");
    const auto* mp3Opts = std::get_if<abracapture::exporting::Mp3Options>(&migrated.target.codecOptions);
    expect(mp3Opts != nullptr && mp3Opts->bitrateKbps == 256,
           "Migrated profile preserves codec options");
}

// ---- Existing ffmpeg argument builder tests (updated for new types) ----

void testMp3Cbr320PreservesSourceRate() {
    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    QStringList arguments;
    expect(build(requestFor(profile), &arguments), "MP3 320 request builds");
    expect(arguments == QStringList{
                            QStringLiteral("-nostdin"), QStringLiteral("-n"),
                            QStringLiteral("-i"), QStringLiteral("/masters/track.wav"),
                            QStringLiteral("-c:a"), QStringLiteral("libmp3lame"),
                            QStringLiteral("-b:a"), QStringLiteral("320k"),
                            QStringLiteral("/exports/track.mp3")},
           "MP3 320 CBR arguments are exact and omit resampling");
    expect(!arguments.contains(QStringLiteral("-q:a")), "MP3 CBR does not use the old VBR quality flag");
}

void testMp3ExplicitSampleRate() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    profile.target.sampleRate = {abracapture::exporting::SampleRatePolicy::Explicit, 44100};
    QStringList arguments;
    expect(build(requestFor(profile), &arguments), "MP3 explicit-rate request builds");
    const qsizetype rateIndex = arguments.indexOf(QStringLiteral("-ar"));
    expect(rateIndex >= 0 && arguments.value(rateIndex + 1) == QStringLiteral("44100"),
           "MP3 explicit sample rate adds -ar 44100");
}

void testMp3RejectsUnsupportedPreserveSourceRate() {
    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    auto request = requestFor(profile);
    request.sourceFormat.sampleRate = 96000;
    QStringList arguments;
    QString error;
    expect(!abracapture::exporting::FfmpegArgumentBuilder::build(request, &arguments, &error),
           "MP3 preserve-source rejects an encoder-unsupported source rate");
    expect(error.contains(QStringLiteral("44.1 or 48")),
           "Unsupported MP3 preserve-source rate provides a useful correction");
}

void testFlacOptions() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
    profile.target.codecOptions = abracapture::exporting::FlacOptions{24, 8};
    QStringList arguments;
    expect(build(requestFor(profile, "/exports/track.flac"), &arguments), "FLAC request builds");
    expect(!arguments.contains(QStringLiteral("-ar")), "FLAC preserve-source omits resampling");
    expect(arguments.contains(QStringLiteral("-compression_level")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-compression_level")) + 1) == QStringLiteral("8"),
           "FLAC compression level is explicit");
    expect(arguments.contains(QStringLiteral("-bits_per_raw_sample")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-bits_per_raw_sample")) + 1) == QStringLiteral("24"),
           "FLAC 24-bit output is explicit");
    expect(arguments.contains(QStringLiteral("-af")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-af")) + 1).contains(
                   QStringLiteral("output_sample_bits=24:dither_method=triangular")),
           "FLAC 24-bit conversion uses explicit triangular dither");
}

void testFlac16ExplicitRateUsesDitheredResample() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
    profile.target.codecOptions = abracapture::exporting::FlacOptions{16, 5};
    profile.target.sampleRate = {abracapture::exporting::SampleRatePolicy::Explicit, 96000};
    QStringList arguments;
    expect(build(requestFor(profile, "/exports/track.flac"), &arguments),
           "FLAC 16-bit explicit-rate request builds");
    const qsizetype filterIndex = arguments.indexOf(QStringLiteral("-af"));
    expect(filterIndex >= 0 && arguments.value(filterIndex + 1).contains(
               QStringLiteral("out_sample_fmt=s16:output_sample_bits=16:dither_method=triangular:out_sample_rate=96000")),
           "FLAC 16-bit conversion combines dither and explicit resampling");
    expect(!arguments.contains(QStringLiteral("-ar")),
           "Dithered FLAC resampling does not add a competing -ar option");

    profile.target.sampleRate = {abracapture::exporting::SampleRatePolicy::PreserveSource, 0};
    profile.target.codecOptions = abracapture::exporting::FlacOptions{32, 5};
    expect(build(requestFor(profile, "/exports/track-32.flac"), &arguments),
           "FLAC 32-bit request builds through ffmpeg's supported s32 encoder path");
    expect(arguments.contains(QStringLiteral("-bits_per_raw_sample")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-bits_per_raw_sample")) + 1) == QStringLiteral("32"),
           "FLAC 32-bit output declares 32 raw sample bits");
    expect(!arguments.contains(QStringLiteral("-af")),
           "FLAC 32-bit output avoids unnecessary lower-bit dithering");
}

void testWavSampleFormat() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Wav, abracapture::exporting::AudioCodec::Pcm);
    profile.target.codecOptions = abracapture::exporting::PcmOptions{
        abracapture::exporting::AudioSampleFormat::SignedInt24};
    QStringList arguments;
    expect(build(requestFor(profile, "/exports/track.wav"), &arguments), "WAV request builds");
    expect(arguments.contains(QStringLiteral("pcm_s24le")), "WAV signed 24-bit uses pcm_s24le");
    expect(arguments.contains(QStringLiteral("-af")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-af")) + 1).contains(
                   QStringLiteral("output_sample_bits=24:dither_method=triangular")),
           "WAV signed 24-bit conversion uses explicit triangular dither");

    profile.target.codecOptions = abracapture::exporting::PcmOptions{
        abracapture::exporting::AudioSampleFormat::SignedInt16};
    expect(build(requestFor(profile, "/exports/track-16.wav"), &arguments),
           "WAV signed 16-bit request builds");
    expect(arguments.contains(QStringLiteral("pcm_s16le")), "WAV signed 16-bit uses pcm_s16le");
    expect(arguments.contains(QStringLiteral("-af")) &&
               arguments.value(arguments.indexOf(QStringLiteral("-af")) + 1).contains(
                   QStringLiteral("output_sample_bits=16:dither_method=triangular")),
           "WAV signed 16-bit conversion uses explicit triangular dither");

    profile.target.codecOptions = abracapture::exporting::PcmOptions{
        abracapture::exporting::AudioSampleFormat::SignedInt32};
    expect(build(requestFor(profile, "/exports/track-32.wav"), &arguments),
           "WAV signed 32-bit request builds");
    expect(arguments.contains(QStringLiteral("pcm_s32le")), "WAV signed 32-bit uses pcm_s32le");
    expect(!arguments.contains(QStringLiteral("-af")), "WAV signed 32-bit does not reduce precision");

    profile.target.codecOptions = abracapture::exporting::PcmOptions{
        abracapture::exporting::AudioSampleFormat::Float32};
    expect(build(requestFor(profile, "/exports/track-float.wav"), &arguments),
           "WAV float request builds");
    expect(arguments.contains(QStringLiteral("pcm_f32le")), "WAV float uses pcm_f32le");
    expect(!arguments.contains(QStringLiteral("-af")), "WAV float preserves float samples without dither");
}

void testMetadataNaming() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary metadata naming directory is available");
    if (!directory.isValid()) {
        return;
    }
    const std::filesystem::path root = directory.path().toStdString();
    abracapture::recording::OutputPathPolicy policy(root, "{source}_{date}_{time}", "wav");

    abracapture::recording::CaptureMetadata metadata;
    metadata.source = "Firefox";
    metadata.format = "wav";
    metadata.automatedCapture = true;
    metadata.useMediaMetadataNames = true;
    metadata.mediaTitle = "Battery";
    metadata.mediaAlbum = "Master of Puppets";
    const std::filesystem::path first = policy.resolveOutputPath(metadata);
    expect(first.parent_path().filename() == "Master of Puppets" && first.filename() == "Battery.wav",
           "Automated metadata capture uses album directory and track filename");

    const std::string sanitized = abracapture::recording::OutputPathPolicy::sanitizePathComponent(
        "../Battery/\\:*?\"<>|\n");
    expect(!sanitized.empty() && sanitized.find('/') == std::string::npos &&
               sanitized.find('\\') == std::string::npos && sanitized != "." && sanitized != "..",
           "Metadata path components are sanitized against traversal and reserved characters");

    abracapture::recording::CaptureMetadata noAlbum = metadata;
    noAlbum.mediaAlbum.clear();
    const std::filesystem::path noAlbumPath = policy.resolveOutputPath(noAlbum);
    expect(noAlbumPath.parent_path() == std::filesystem::absolute(root) &&
               noAlbumPath.filename() == "Battery.wav",
           "Missing album metadata keeps automated capture in configured root");

    abracapture::recording::CaptureMetadata noTrack = metadata;
    noTrack.mediaTitle.clear();
    noTrack.mediaName.clear();
    const std::filesystem::path noTrackPath = policy.resolveOutputPath(noTrack);
    expect(noTrackPath.parent_path().filename() == "Master of Puppets" &&
               noTrackPath.filename().string().starts_with("Firefox_"),
           "Missing track metadata falls back to the configured generated filename");

    QFile occupied(QString::fromStdString(first.string()));
    expect(occupied.open(QIODevice::WriteOnly), "Metadata collision fixture can be created");
    occupied.close();
    const std::filesystem::path repeated = policy.resolveOutputPath(metadata);
    expect(repeated.filename() == "Battery_001.wav",
           "Repeated metadata track names use a non-destructive numeric suffix");

    abracapture::recording::CaptureMetadata manual = metadata;
    manual.automatedCapture = false;
    const std::filesystem::path manualPath = policy.resolveOutputPath(manual);
    expect(manualPath.parent_path() == std::filesystem::absolute(root) &&
               manualPath.filename().string().starts_with("Firefox_"),
           "Manual capture does not move into metadata album folders");
}

void testInvalidProfileIsRejected() {
    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    profile.target.codecOptions = abracapture::exporting::FlacOptions{};
    QString error;
    expect(!profile.validate(&error), "Mismatched format options are rejected");
    expect(!error.isEmpty(), "Invalid profile supplies a diagnostic");

    QStringList arguments;
    expect(!abracapture::exporting::FfmpegArgumentBuilder::build(
               requestFor(profile), &arguments, &error),
           "Argument builder rejects invalid profile");
}

void testOutputPlanningAvoidsCollisions() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary export directory is available");
    if (!directory.isValid()) {
        return;
    }

    const std::filesystem::path desired =
        std::filesystem::path(directory.path().toStdString()) / "track.mp3";
    QFile occupied(QString::fromStdString(desired.string()));
    expect(occupied.open(QIODevice::WriteOnly), "Collision fixture can be created");
    occupied.close();

    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    abracapture::exporting::ExportRequest planned;
    QString error;
    expect(abracapture::exporting::ExportOutputPlanner::plan(
               requestFor(profile, desired), {}, &planned, &error),
           "Collision planner accepts a unique-name policy");
    expect(planned.destinationPath.filename() == "track_001.mp3",
           "Collision planner selects the expected numeric suffix");

    auto failProfile = profile;
    failProfile.collisionPolicy = abracapture::exporting::CollisionPolicy::FailIfExists;
    expect(!abracapture::exporting::ExportOutputPlanner::plan(
               requestFor(failProfile, desired), {}, &planned, &error),
           "Fail-if-exists collision policy rejects occupied output");

    auto replaceProfile = profile;
    replaceProfile.collisionPolicy = abracapture::exporting::CollisionPolicy::ReplaceExisting;
    expect(abracapture::exporting::ExportOutputPlanner::plan(
               requestFor(replaceProfile, desired), {}, &planned, &error) &&
               planned.destinationPath == desired,
           "Replace-existing requires an explicit policy and retains the requested path");
}

void testManualProfileModesAndBulkPreflight() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Manual profile fixture directory is available");
    if (!directory.isValid()) return;
    const std::filesystem::path root = directory.path().toStdString();
    const auto first = root / "first.wav";
    const auto second = root / "second.wav";
    expect(writeSilenceWav(first) && writeSilenceWav(second), "Bulk profile WAV fixtures can be created");
    QFile existing(QString::fromStdString((root / "first.mp3").string()));
    expect(existing.open(QIODevice::WriteOnly), "Existing derivative fixture can be created");
    existing.write("existing", 8);
    existing.close();

    abracapture::exporting::ExportManager manager;
    SettingsViewModel settings;
    const auto automaticBefore = settings.automaticExportProfile().profile;
    ManualExportViewModel manual(&manager, nullptr, &settings);
    expect(manual.profileMode() == QStringLiteral("automatic"),
           "Manual export starts with an explicit Automatic Profile mode");
    expect(!manual.activateSelectedRecordings() && manual.sourceMode() == QStringLiteral("recordings") &&
               manual.sourceCount() == 0 && manual.statusMessage().contains(QStringLiteral("Recordings page")),
           "Empty Selected Recordings mode gives direct navigation guidance without stale sources");
    manual.setProfileMode(QStringLiteral("manual"));
    manual.setManualFormat(QStringLiteral("flac"));
    manual.setManualSampleRate(44100);
    manual.setManualFlacBitDepth(16);
    expect(manual.profileMode() == QStringLiteral("manual") &&
               manual.profileSummary().contains(QStringLiteral("FLAC")) &&
               manual.profileSummary().contains(QStringLiteral("16-bit")),
           "Custom manual FLAC profile is independent and visible");
    const auto* automaticMp3 = std::get_if<abracapture::exporting::Mp3Options>(
        &automaticBefore.target.codecOptions);
    const auto automaticAfter = settings.automaticExportProfile().profile;
    const auto* unchangedAutomaticMp3 = std::get_if<abracapture::exporting::Mp3Options>(
        &automaticAfter.target.codecOptions);
    expect(automaticMp3 != nullptr && unchangedAutomaticMp3 != nullptr &&
               automaticMp3->bitrateKbps == unchangedAutomaticMp3->bitrateKbps,
           "Changing the manual profile does not alter automatic export settings");
    manual.setManualFormat(QStringLiteral("wav"));
    manual.setManualWavSampleFormat(QStringLiteral("s24"));
    expect(manual.profileSummary().contains(QStringLiteral("WAV")) &&
               manual.profileSummary().contains(QStringLiteral("24-bit")),
           "Custom manual WAV profile is selectable");

    manual.setManualFormat(QStringLiteral("mp3"));
    manual.setManualMp3Bitrate(320);
    manual.setSelectedRecordings({
        QVariantMap{{QStringLiteral("filePath"), QString::fromStdString(first.string())}},
        QVariantMap{{QStringLiteral("filePath"), QString::fromStdString(second.string())}}
    });
    expect(manual.sourceCount() == 2, "Multiple recordings become one manual source set");
    expect(manual.collisionPolicy() == QStringLiteral("skip"),
           "Bulk manual export defaults to Skip Existing");
    const QVariantMap preview = manual.bulkPreview();
    expect(preview.value(QStringLiteral("skippedCount")).toInt() == 1 &&
               preview.value(QStringLiteral("newCount")).toInt() == 1,
           "Bulk preflight counts existing and new outputs before enqueue");
    expect(manual.enqueueExport() == 1 && manager.jobs().size() == 1,
           "Skipped existing output is not submitted to ffmpeg");
    manual.setCollisionPolicy(QStringLiteral("unique"));
    expect(manual.bulkPreview().value(QStringLiteral("newCount")).toInt() == 2,
           "Explicit unique-name policy plans both bulk outputs");
}

void testQueueAcceptsMultiplePendingJobs() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary queue directory is available");
    if (!directory.isValid()) {
        return;
    }

    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path sourcePath = root / "master.wav";
    QFile source(QString::fromStdString(sourcePath.string()));
    expect(source.open(QIODevice::WriteOnly), "Queue source fixture can be created");
    source.write("master", 6);
    source.close();

    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    auto first = requestFor(profile, root / "first.mp3");
    auto second = requestFor(profile, root / "first.mp3");
    first.sourcePath = sourcePath;
    second.sourcePath = sourcePath;

    abracapture::exporting::ExportManager manager;
    const QList<abracapture::exporting::ExportJobId> jobIds = manager.enqueueAll({first, second});
    expect(jobIds.size() == 2 && jobIds.at(0) != jobIds.at(1),
           "Queue assigns unique IDs to both submitted jobs");
    expect(manager.queuedJobCount() == 2 && manager.isBusy(),
           "Queue retains a second job instead of rejecting it");
    const auto firstJob = manager.job(jobIds.at(0));
    const auto secondJob = manager.job(jobIds.at(1));
    expect(firstJob.has_value() && secondJob.has_value() &&
               firstJob->state == abracapture::exporting::ExportJobState::Queued &&
               secondJob->state == abracapture::exporting::ExportJobState::Queued,
           "Both jobs remain queued until asynchronous processing starts");
    expect(firstJob.has_value() && secondJob.has_value() &&
               firstJob->request.destinationPath.filename() == "first.mp3" &&
               secondJob->request.destinationPath.filename() == "first_001.mp3",
           "Queued jobs reserve distinct collision-safe output paths");
}

void testManualDirectoryEnumerationAndOutputPlanning() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary manual-export directory is available");
    if (!directory.isValid()) return;

    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path rootMaster = root / "root.wav";
    const std::filesystem::path nestedDirectory = root / "nested";
    const std::filesystem::path nestedMaster = nestedDirectory / "nested.flac";
    const std::filesystem::path exportsDirectory = root / "Exports";
    const std::filesystem::path generatedExport = exportsDirectory / "root.flac";
    std::filesystem::create_directory(nestedDirectory);
    std::filesystem::create_directory(exportsDirectory);
    QFile rootFile{QString::fromStdString(rootMaster.string())};
    expect(rootFile.open(QIODevice::WriteOnly), "Root master fixture can be created");
    rootFile.write("master", 6);
    rootFile.close();
    QFile nestedFile(QString::fromStdString(nestedMaster.string()));
    expect(nestedFile.open(QIODevice::WriteOnly), "Nested master fixture can be created");
    nestedFile.write("master", 6);
    nestedFile.close();
    QFile generatedFile(QString::fromStdString(generatedExport.string()));
    expect(generatedFile.open(QIODevice::WriteOnly), "Generated export fixture can be created");
    generatedFile.write("export", 6);
    generatedFile.close();

    const auto nonRecursive = abracapture::exporting::ManualExportPlanner::enumerateDirectory(root, false);
    expect(nonRecursive.size() == 1 && nonRecursive.front().path.filename() == "root.wav",
           "Directory export is non-recursive by default");
    const auto recursive = abracapture::exporting::ManualExportPlanner::enumerateDirectory(root, true);
    expect(recursive.size() == 2, "Directory export includes nested masters only when explicitly enabled");
    bool includesGeneratedExport = false;
    for (const auto& candidate : recursive) {
        if (candidate.path == generatedExport) includesGeneratedExport = true;
    }
    expect(!includesGeneratedExport,
           "Exports subdirectory is excluded from recursive input enumeration");
    expect(abracapture::exporting::ManualExportPlanner::supportedInputExtensions() ==
               QStringList{QStringLiteral("wav"), QStringLiteral("flac")},
           "Manual directory inputs use the centralized master-format list");

    auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    abracapture::exporting::ManualExportSource source;
    source.path = rootMaster;
    source.sourceFormat.sampleRate = 44100;
    source.trackTitle = QStringLiteral("Battery");
    source.albumTitle = QStringLiteral("Master of Puppets");
    abracapture::exporting::ManualExportDestination destination;
    destination.policy = abracapture::exporting::ManualExportDestinationPolicy::ExportsSubfolder;
    destination.useStoredMetadataNames = true;
    destination.organizeAlbumDirectory = true;
    abracapture::exporting::ExportRequest request;
    QString error;
    expect(abracapture::exporting::ManualExportPlanner::planRequest(
               source, profile, destination, false, &request, &error),
           "Manual metadata export request plans without touching the filesystem");
    expect(request.destinationPath == root / "Exports" / "Master of Puppets" / "Battery.mp3",
           "Manual metadata export uses the explicit album organization policy");

    destination.useStoredMetadataNames = false;
    destination.organizeAlbumDirectory = false;
    expect(abracapture::exporting::ManualExportPlanner::planRequest(
               source, profile, destination, false, &request, &error),
           "Manual filename-preserving request plans");
    expect(request.destinationPath == root / "Exports" / "root.mp3",
           "Manual export preserves the source filename by default");
}

void testQueuedJobCanBeCancelled() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary cancellation directory is available");
    if (!directory.isValid()) return;
    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path sourcePath = root / "master.wav";
    QFile source(QString::fromStdString(sourcePath.string()));
    expect(source.open(QIODevice::WriteOnly), "Cancellation source fixture can be created");
    source.write("master", 6);
    source.close();
    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    auto request = requestFor(profile, root / "cancel.mp3");
    request.sourcePath = sourcePath;
    abracapture::exporting::ExportManager manager;
    const auto jobId = manager.enqueue(request);
    expect(manager.cancelQueued(jobId), "Queued export can be cancelled safely");
    const auto job = manager.job(jobId);
    expect(job.has_value() && job->state == abracapture::exporting::ExportJobState::Cancelled &&
               manager.queuedJobCount() == 0,
           "Cancelled export leaves the shared queue intact");
}

void testCaptureSessionResolutionAndRetry() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary manual session directory is available");
    if (!directory.isValid()) return;
    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path older = root / "older.wav";
    const std::filesystem::path recent = root / "recent.wav";
    QFile olderFile{QString::fromStdString(older.string())};
    QFile recentFile{QString::fromStdString(recent.string())};
    expect(olderFile.open(QIODevice::WriteOnly) && recentFile.open(QIODevice::WriteOnly),
           "Session master fixtures can be created");
    olderFile.write("master", 6); recentFile.write("master", 6);
    olderFile.close(); recentFile.close();

    abracapture::history::RecordingIndex index(QString::fromStdString((root / "history.json").string()));
    abracapture::history::RecordingEntry first;
    first.filePath = QString::fromStdString(older.string());
    first.captureSessionId = QStringLiteral("older-session");
    first.timestamp = QDateTime::currentDateTimeUtc().addSecs(-60);
    first.trackTitle = QStringLiteral("Older Track");
    abracapture::history::RecordingEntry second;
    second.filePath = QString::fromStdString(recent.string());
    second.captureSessionId = QStringLiteral("recent-session");
    second.timestamp = QDateTime::currentDateTimeUtc();
    second.trackTitle = QStringLiteral("Recent Track");
    second.albumTitle = QStringLiteral("Recent Album");
    expect(index.append(first) && index.append(second), "Session fixtures are indexed");

    abracapture::exporting::ExportManager manager;
    ManualExportViewModel manual(&manager, &index, nullptr);
    expect(manual.captureSessions().size() == 2, "Indexed sessions resolve by stable capture-session ID");
    expect(manual.selectMostRecentCaptureSession() && manual.sourceCount() == 1 &&
               manual.selectedSessionId() == QStringLiteral("recent-session"),
           "Most recent capture session resolves its own master without filename grouping");

    const std::filesystem::path invalidWav = root / "invalid.wav";
    QFile invalidFile{QString::fromStdString(invalidWav.string())};
    expect(invalidFile.open(QIODevice::WriteOnly), "Invalid audio retry fixture can be created");
    invalidFile.write("not audio", 9); invalidFile.close();
    expect(manual.selectFile(QString::fromStdString(invalidWav.string())), "Manual file selection accepts supported extension");
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&manager, &abracapture::exporting::ExportManager::jobFailed,
                     &loop, [&loop](const abracapture::exporting::ExportJobId, const QString&) { loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    expect(manual.enqueueExport() == 1, "Manual file export enters the shared queue");
    timeout.start(10000);
    loop.exec();
    const QVariantList jobs = manual.jobs();
    expect(!jobs.isEmpty() && jobs.constFirst().toMap().value(QStringLiteral("status")) == QStringLiteral("Failed"),
           "Failed manual export remains visible");
    const qulonglong failedId = jobs.constFirst().toMap().value(QStringLiteral("jobId")).toULongLong();
    expect(manual.retry(failedId) && manual.jobs().size() == 2,
           "Retry creates a new shared-queue job from the failed request");
}

void testManualFileWorkflowCompletes() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary manual-file directory is available");
    if (!directory.isValid()) return;
    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path sourcePath = root / "manual.wav";
    expect(writeSilenceWav(sourcePath), "Manual-file WAV fixture can be created");

    abracapture::exporting::ExportManager manager;
    ManualExportViewModel manual(&manager, nullptr, nullptr);
    expect(manual.selectFile(QString::fromStdString(sourcePath.string())),
           "Manual file mode accepts a supported master");
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    QObject::connect(&manager, &abracapture::exporting::ExportManager::jobCompleted,
                     &loop, [&completed, &loop](const abracapture::exporting::ExportJobId,
                                                 const QString&) { completed = true; loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    expect(manual.enqueueExport() == 1, "Manual file mode queues one shared export job");
    timeout.start(10000);
    loop.exec();
    const QVariantList jobs = manual.jobs();
    expect(completed && jobs.size() == 1 &&
               jobs.constFirst().toMap().value(QStringLiteral("status")) == QStringLiteral("Complete"),
           "Manual file job completes through the shared export manager");
    expect(QFileInfo::exists(QString::fromStdString(sourcePath.string())) &&
               QFileInfo::exists(QString::fromStdString((root / "manual.mp3").string())),
           "Manual file export creates a derivative without removing the master");
}

void testManagerGeneratesMp3320() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Temporary ffmpeg directory is available");
    if (!directory.isValid()) {
        return;
    }

    const std::filesystem::path root = directory.path().toStdString();
    const std::filesystem::path sourcePath = root / "master.wav";
    const std::filesystem::path destinationPath = root / "master.mp3";
    expect(writeSilenceWav(sourcePath), "Synthetic WAV source can be created");
    if (!QFileInfo::exists(QString::fromStdString(sourcePath.string()))) {
        return;
    }

    auto request = requestFor(
        abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3),
        destinationPath);
    request.sourcePath = sourcePath;

    abracapture::exporting::ExportManager manager;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    bool failed = false;
    QString outputPath;
    QObject::connect(&manager, &abracapture::exporting::ExportManager::jobCompleted,
                     &loop, [&](const abracapture::exporting::ExportJobId, const QString& output) {
                         completed = true;
                         outputPath = output;
                         loop.quit();
                     });
    QObject::connect(&manager, &abracapture::exporting::ExportManager::jobFailed,
                     &loop, [&](const abracapture::exporting::ExportJobId, const QString&) {
                         failed = true;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    const auto jobId = manager.enqueue(request);
    const auto queuedJob = manager.job(jobId);
    expect(queuedJob.has_value(), "Real export job is created");
    if (queuedJob.has_value() && queuedJob->state == abracapture::exporting::ExportJobState::Queued) {
        timeout.start(10000);
        loop.exec();
    }

    expect(completed && !failed, "ffmpeg completes the MP3 export");
    expect(QFileInfo(outputPath).exists() && QFileInfo(outputPath).size() > 0,
           "Completed MP3 output is non-empty");
    expect(QFileInfo::exists(QString::fromStdString(sourcePath.string())),
           "Master WAV remains intact after export");
    if (!completed) {
        return;
    }

    QProcess ffprobe;
    ffprobe.start(QStringLiteral("ffprobe"), {
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-show_entries"), QStringLiteral("stream=codec_name,sample_rate,bit_rate"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1"), outputPath
    });
    expect(ffprobe.waitForFinished(5000), "ffprobe inspects the generated MP3");
    const QString probeOutput = QString::fromLocal8Bit(ffprobe.readAllStandardOutput());
    expect(probeOutput.contains(QStringLiteral("codec_name=mp3")),
           "Generated export reports the MP3 codec");
    expect(probeOutput.contains(QStringLiteral("sample_rate=44100")),
           "Generated export preserves the source sample rate");
    expect(probeOutput.contains(QStringLiteral("bit_rate=320000")),
           "Generated export reports the requested 320 kbps bitrate");
}

// ---- DerivedExport serialization ----

void testDerivedExportSerialization() {
    abracapture::history::DerivedExport original;
    original.jobId = 42;
    original.format = QStringLiteral("mp3");
    original.container = QStringLiteral("mp3");
    original.codec = QStringLiteral("mp3");
    original.sampleRate = 44100;
    original.bitDepth = 0;
    original.bitrate = 320;
    original.outputPath = QStringLiteral("/exports/track.mp3");
    original.state = QStringLiteral("completed");
    original.error = QString();
    const auto json = original.toJson();
    const auto restored = abracapture::history::DerivedExport::fromJson(json);
    expect(restored == original, "DerivedExport round-trips through JSON with new fields");

    QJsonObject oldFormat;
    oldFormat[QStringLiteral("jobId")] = 7;
    oldFormat[QStringLiteral("format")] = QStringLiteral("flac");
    oldFormat[QStringLiteral("outputPath")] = QStringLiteral("/exports/old.flac");
    oldFormat[QStringLiteral("state")] = QStringLiteral("completed");
    oldFormat[QStringLiteral("error")] = QString();
    const auto legacy = abracapture::history::DerivedExport::fromJson(oldFormat);
    expect(legacy.container.isEmpty() && legacy.codec.isEmpty() && legacy.sampleRate == 0,
           "Old DerivedExport JSON without container/codec fields deserializes gracefully");

    const auto profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
    const auto req = requestFor(profile, "/exports/track.mp3");
    const auto derivedRecord = abracapture::exporting::createDerivedExportRecord(
        42, req, QStringLiteral("completed"), QStringLiteral("/exports/track.mp3"), QString());
    expect(derivedRecord.container == QStringLiteral("mp3") && derivedRecord.codec == QStringLiteral("mp3") &&
           derivedRecord.bitrate == 320,
           "createDerivedExportRecord populates container, codec, and bitrate");
}

// ---- New Formats and Multichannel Surround Tests ----

void testNewFormatArgumentBuilders() {
    // 1. AIFF 24-bit PCM
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Aiff, abracapture::exporting::AudioCodec::Pcm);
        profile.target.codecOptions = abracapture::exporting::PcmOptions{
            abracapture::exporting::AudioSampleFormat::SignedInt24};
        QStringList args;
        expect(build(requestFor(profile, "/exports/out.aiff"), &args), "AIFF 24-bit PCM builds");
        expect(args.contains(QStringLiteral("pcm_s24be")), "AIFF uses big-endian PCM pcm_s24be");
        expect(args.contains(QStringLiteral("aiff")), "AIFF specifies -f aiff");
    }

    // 2. ALAC Lossless (M4A)
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::M4a, abracapture::exporting::AudioCodec::Alac);
        QStringList args;
        expect(build(requestFor(profile, "/exports/out.m4a"), &args), "ALAC Lossless builds");
        expect(args.contains(QStringLiteral("alac")), "ALAC specifies -c:a alac");
        expect(args.contains(QStringLiteral("ipod")), "ALAC specifies -f ipod");
    }

    // 3. AAC 256k (M4A)
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::M4a, abracapture::exporting::AudioCodec::Aac);
        profile.target.codecOptions = abracapture::exporting::AacOptions{256};
        QStringList args;
        expect(build(requestFor(profile, "/exports/out.m4a"), &args), "AAC 256k builds");
        expect(args.contains(QStringLiteral("aac")), "AAC specifies -c:a aac");
        expect(args.contains(QStringLiteral("256k")), "AAC specifies 256k bitrate");
        expect(args.contains(QStringLiteral("ipod")), "AAC specifies -f ipod");
    }

    // 4. WavPack Lossless (WV)
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::WavPack, abracapture::exporting::AudioCodec::WavPack);
        QStringList args;
        expect(build(requestFor(profile, "/exports/out.wv"), &args), "WavPack Lossless builds");
        expect(args.contains(QStringLiteral("wavpack")), "WavPack specifies -c:a wavpack");
        expect(args.contains(QStringLiteral("wv")), "WavPack specifies -f wv");
    }

    // 5. CAF 24-bit PCM
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Caf, abracapture::exporting::AudioCodec::Pcm);
        profile.target.codecOptions = abracapture::exporting::PcmOptions{
            abracapture::exporting::AudioSampleFormat::SignedInt24};
        QStringList args;
        expect(build(requestFor(profile, "/exports/out.caf"), &args), "CAF 24-bit PCM builds");
        expect(args.contains(QStringLiteral("pcm_s24le")), "CAF specifies pcm_s24le");
        expect(args.contains(QStringLiteral("caf")), "CAF specifies -f caf");
    }

    // 6. Opus 160k (OPUS)
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Opus, abracapture::exporting::AudioCodec::Opus);
        profile.target.codecOptions = abracapture::exporting::OpusOptions{160};
        QStringList args;
        expect(build(requestFor(profile, "/exports/out.opus"), &args), "Opus 160k builds");
        expect(args.contains(QStringLiteral("libopus")), "Opus specifies -c:a libopus");
        expect(args.contains(QStringLiteral("160k")), "Opus specifies 160k bitrate");
        expect(args.contains(QStringLiteral("opus")), "Opus specifies -f opus");
    }

    // 7. AC-3 5.1 (640 kbps)
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Ac3, abracapture::exporting::AudioCodec::Ac3);
        profile.target.codecOptions = abracapture::exporting::Ac3Options{640};
        auto req = requestFor(profile, "/exports/out.ac3");
        req.sourceFormat.sampleRate = 48000;
        req.sourceFormat.channelLayout = abracapture::exporting::AudioChannelLayout::surround51();
        QStringList args;
        expect(build(req, &args), "AC-3 5.1 request builds with explicit 5.1 layout");
        expect(args.contains(QStringLiteral("ac3")), "AC-3 specifies -c:a ac3");
        expect(args.contains(QStringLiteral("640k")), "AC-3 specifies 640k bitrate");
    }

    // 8. E-AC-3 5.1 (768 kbps)
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Eac3, abracapture::exporting::AudioCodec::Eac3);
        profile.target.codecOptions = abracapture::exporting::Eac3Options{768};
        auto req = requestFor(profile, "/exports/out.eac3");
        req.sourceFormat.sampleRate = 48000;
        req.sourceFormat.channelLayout = abracapture::exporting::AudioChannelLayout::surround51();
        QStringList args;
        expect(build(req, &args), "E-AC-3 5.1 request builds with explicit 5.1 layout");
        expect(args.contains(QStringLiteral("eac3")), "E-AC-3 specifies -c:a eac3");
        expect(args.contains(QStringLiteral("768k")), "E-AC-3 specifies 768k bitrate");
    }
}

void testMultichannelSurroundValidation() {
    // AC-3: unknown layout with 6 channels must be rejected
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Ac3, abracapture::exporting::AudioCodec::Ac3);
        auto req = requestFor(profile, "/exports/out.ac3");
        req.sourceFormat.sampleRate = 48000;
        req.sourceFormat.channelLayout = abracapture::exporting::AudioChannelLayout::unknown(6);
        QStringList args;
        QString error;
        const bool success = abracapture::exporting::FfmpegArgumentBuilder::build(req, &args, &error);
        expect(!success, "AC-3 surround export with Unknown(6) is rejected");
        expect(error.contains(QStringLiteral("surround export requires known speaker positions")),
               "Rejection reason clearly identifies unknown speaker positions");
    }

    // AC-3: 7.1 surround must be rejected (AC-3 max 5.1)
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Ac3, abracapture::exporting::AudioCodec::Ac3);
        auto req = requestFor(profile, "/exports/out.ac3");
        req.sourceFormat.sampleRate = 48000;
        req.sourceFormat.channelLayout = abracapture::exporting::AudioChannelLayout::surround71();
        QStringList args;
        QString error;
        const bool success = abracapture::exporting::FfmpegArgumentBuilder::build(req, &args, &error);
        expect(!success, "AC-3 export with 7.1 surround is rejected");
        expect(error.contains(QStringLiteral("does not support 7.1")),
               "Rejection reason states AC-3 does not support 7.1");
    }

    // E-AC-3: 7.1 surround with explicit positions MUST be capability-gated.
    //
    // The bundled FFmpeg's eac3 encoder tops out at 5.1 in its open-source
    // `acmod` table, and Dolby does not publish bit-allocation tables for 7.1
    // in any free encoder. So a 7.1 E-AC-3 build attempt on this machine
    // must FAIL with a precise backend-capability error, never a silent
    // downgrade and never a claim of fake support. We exercise that path
    // here by reading the live capability probe result — if it claims 7.1
    // support (for example with a future backend integration), the build
    // proceeds; otherwise it must fail loudly.
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Eac3, abracapture::exporting::AudioCodec::Eac3);
        auto req = requestFor(profile, "/exports/out.eac3");
        req.sourceFormat.sampleRate = 48000;
        req.sourceFormat.channelLayout = abracapture::exporting::AudioChannelLayout::surround71();
        QStringList args;
        QString error;
        const auto capability = abracapture::exporting::Eac3Capability::probeNow();
        const bool success = abracapture::exporting::FfmpegArgumentBuilder::build(
            req, capability, &args, &error);
        if (capability.supports71()) {
            expect(success, "E-AC-3 export with 7.1 surround succeeds when capability proves support");
            expect(args.contains(QStringLiteral("eac3")), "E-AC-3 7.1 path emits -c:a eac3");
            expect(args.contains(QStringLiteral("7.1")), "E-AC-3 7.1 path emits explicit -channel_layout 7.1");
            expect(args.contains(QStringLiteral("8")), "E-AC-3 7.1 path emits explicit -ac 8");
        } else {
            expect(!success,
                   "E-AC-3 export with 7.1 surround is refused without proving real 8-channel support");
            expect(!error.isEmpty(),
                   "E-AC-3 export with 7.1 surround produces an explicit error explaining the external limitation");
            expect(error.contains(QStringLiteral("7.1")) || error.contains(QStringLiteral("8-channel")),
                   "E-AC-3 export with 7.1 surround error message names the missing 8-channel capability");
            expect(!error.contains(QStringLiteral("downgrade")),
                   "E-AC-3 export with 7.1 surround error never mentions a downgrade");
        }
    }

    // AC-3: unsupported preserve-source rate (96 kHz) is rejected
    {
        auto profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::AudioContainer::Ac3, abracapture::exporting::AudioCodec::Ac3);
        auto req = requestFor(profile, "/exports/out.ac3");
        req.sourceFormat.sampleRate = 96000;
        req.sourceFormat.channelLayout = abracapture::exporting::AudioChannelLayout::surround51();
        QStringList args;
        QString error;
        const bool success = abracapture::exporting::FfmpegArgumentBuilder::build(req, &args, &error);
        expect(!success, "AC-3 with 96 kHz source rate is rejected when preserving source rate");
        expect(error.contains(QStringLiteral("requires a sample rate of 32, 44.1, or 48 kHz")),
               "Rejection reason states valid AC-3 sample rates");
    }
}

void testRealEncodingMatrix() {
    QTemporaryDir dir;
    expect(dir.isValid(), "Temporary directory created for real encoding matrix");
    if (!dir.isValid()) return;

    const std::filesystem::path root = dir.path().toStdString();
    const std::filesystem::path sourceWav = root / "source_51.wav";

    // Write a 48 kHz 6-channel 5.1 synthetic WAV
    expect(writeMultichannelSilenceWav(sourceWav, 6, 48000,
        {abracapture::exporting::AudioChannelPosition::FrontLeft,
         abracapture::exporting::AudioChannelPosition::FrontRight,
         abracapture::exporting::AudioChannelPosition::FrontCenter,
         abracapture::exporting::AudioChannelPosition::Lfe,
         abracapture::exporting::AudioChannelPosition::RearLeft,
         abracapture::exporting::AudioChannelPosition::RearRight}),
        "Synthetic 5.1 WAV written");

    abracapture::exporting::FfmpegCapabilities capabilities;
    static_cast<void>(capabilities.probe());
    if (!capabilities.isProbed()) {
        std::cerr << "  (skipping real encoding matrix — ffmpeg not probed)\n";
        return;
    }

    struct TargetTest {
        abracapture::exporting::AudioContainer container;
        abracapture::exporting::AudioCodec codec;
        QString expectedCodecName;
        int expectedChannels;
    };

    const QList<TargetTest> testsToRun = {
        {abracapture::exporting::AudioContainer::Aiff, abracapture::exporting::AudioCodec::Pcm, QStringLiteral("pcm_s16be"), 6},
        {abracapture::exporting::AudioContainer::M4a, abracapture::exporting::AudioCodec::Alac, QStringLiteral("alac"), 6},
        {abracapture::exporting::AudioContainer::M4a, abracapture::exporting::AudioCodec::Aac, QStringLiteral("aac"), 6},
        {abracapture::exporting::AudioContainer::WavPack, abracapture::exporting::AudioCodec::WavPack, QStringLiteral("wavpack"), 6},
        {abracapture::exporting::AudioContainer::Caf, abracapture::exporting::AudioCodec::Pcm, QStringLiteral("pcm_s16le"), 6},
        {abracapture::exporting::AudioContainer::Opus, abracapture::exporting::AudioCodec::Opus, QStringLiteral("opus"), 6},
        {abracapture::exporting::AudioContainer::Ac3, abracapture::exporting::AudioCodec::Ac3, QStringLiteral("ac3"), 6},
        {abracapture::exporting::AudioContainer::Eac3, abracapture::exporting::AudioCodec::Eac3, QStringLiteral("eac3"), 6},
        {abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac, QStringLiteral("flac"), 6},
        {abracapture::exporting::AudioContainer::Wav, abracapture::exporting::AudioCodec::Pcm, QStringLiteral("pcm_s16le"), 6},
        {abracapture::exporting::AudioContainer::Ogg, abracapture::exporting::AudioCodec::Vorbis, QStringLiteral("vorbis"), 6},
    };

    for (const auto& test : testsToRun) {
        if (!capabilities.isTargetAvailable(test.container, test.codec)) {
            std::cout << "  (skipping unsupported target: "
                      << abracapture::exporting::audioContainerName(test.container).toStdString() << ")\n";
            continue;
        }

        const auto profile = abracapture::exporting::defaultExportProfile(test.container, test.codec);
        const std::filesystem::path dest = root / ("out_" + abracapture::exporting::audioContainerName(test.container).toStdString() +
                                                   "." + profile.target.fileExtension().toStdString());
        auto req = requestFor(profile, dest);
        req.sourcePath = sourceWav;
        req.sourceFormat.sampleRate = 48000;
        req.sourceFormat.channelLayout = abracapture::exporting::AudioChannelLayout::surround51();

        abracapture::exporting::ExportManager manager;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        bool completed = false;
        bool failed = false;
        QString outputPath;
        QString failedError;

        QObject::connect(&manager, &abracapture::exporting::ExportManager::jobCompleted,
                         &loop, [&](const abracapture::exporting::ExportJobId, const QString& output) {
                             completed = true;
                             outputPath = output;
                             loop.quit();
                         });
        QObject::connect(&manager, &abracapture::exporting::ExportManager::jobFailed,
                         &loop, [&](const abracapture::exporting::ExportJobId, const QString& error) {
                             failed = true;
                             failedError = error;
                             loop.quit();
                         });
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

        const auto jobId = manager.enqueue(req);
        timeout.start(10000);
        loop.exec();

        if (failed) {
            std::cerr << "Export failed for " << abracapture::exporting::audioContainerName(test.container).toStdString()
                      << ": " << failedError.toStdString() << "\n";
        }
        expect(completed && !failed, ("Real ffmpeg encoding succeeds for " + abracapture::exporting::audioContainerName(test.container).toStdString()).c_str());
        expect(QFileInfo::exists(outputPath) && QFileInfo(outputPath).size() > 0,
               ("Export output is non-empty for " + abracapture::exporting::audioContainerName(test.container).toStdString()).c_str());

        if (completed) {
            QProcess ffprobe;
            ffprobe.start(QStringLiteral("ffprobe"), {
                QStringLiteral("-v"), QStringLiteral("error"),
                QStringLiteral("-show_entries"), QStringLiteral("stream=codec_name,sample_rate,channels"),
                QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1"), outputPath
            });
            expect(ffprobe.waitForFinished(5000), "ffprobe inspects the generated output");
            const QString probeOutput = QString::fromLocal8Bit(ffprobe.readAllStandardOutput());
            expect(probeOutput.contains("codec_name=" + test.expectedCodecName),
                   ("ffprobe verifies codec name " + test.expectedCodecName.toStdString()).c_str());
            expect(probeOutput.contains(QStringLiteral("channels=6")),
                   "ffprobe verifies 6 audio channels are preserved");
        }
    }
}

void testMultichannelWavPersistenceAndLayoutPropagation() {
    QTemporaryDir dir;
    expect(dir.isValid(), "Temporary directory created for persistence test");
    if (!dir.isValid()) return;

    const std::filesystem::path root = dir.path().toStdString();
    const std::filesystem::path master51 = root / "multichannel_master.wav";
    const std::filesystem::path master71 = root / "multichannel_71_master.wav";

    expect(writeMultichannelSilenceWav(master51, 6, 48000,
        {abracapture::exporting::AudioChannelPosition::FrontLeft,
         abracapture::exporting::AudioChannelPosition::FrontRight,
         abracapture::exporting::AudioChannelPosition::FrontCenter,
         abracapture::exporting::AudioChannelPosition::Lfe,
         abracapture::exporting::AudioChannelPosition::RearLeft,
         abracapture::exporting::AudioChannelPosition::RearRight}),
        "Multichannel 5.1 WAV written with channel map");

    expect(writeMultichannelSilenceWav(master71, 8, 48000,
        {abracapture::exporting::AudioChannelPosition::FrontLeft,
         abracapture::exporting::AudioChannelPosition::FrontRight,
         abracapture::exporting::AudioChannelPosition::FrontCenter,
         abracapture::exporting::AudioChannelPosition::Lfe,
         abracapture::exporting::AudioChannelPosition::RearLeft,
         abracapture::exporting::AudioChannelPosition::RearRight,
         abracapture::exporting::AudioChannelPosition::SideLeft,
         abracapture::exporting::AudioChannelPosition::SideRight}),
        "Multichannel 7.1 WAV written with channel map");

    abracapture::history::RecordingIndex index((dir.path() + QStringLiteral("/history.json")));
    expect(index.importDirectory(dir.path()), "RecordingIndex imports directory with multichannel files");
    expect(index.count() == 2, "Two recording entries imported (5.1 and 7.1)");

    abracapture::history::RecordingEntry entry51;
    abracapture::history::RecordingEntry entry71;
    for (qsizetype i = 0; i < index.count(); ++i) {
        const auto entry = index.at(i);
        expect(entry.has_value(), "Index entry retrieved");
        if (!entry.has_value()) continue;
        if (entry->channels == 6) {
            entry51 = *entry;
        } else if (entry->channels == 8) {
            entry71 = *entry;
        }
    }
    expect(entry51.channels == 6, "5.1 entry channels is 6");
    expect(entry51.channelLayout == QStringLiteral("5.1"),
           "5.1 entry channelLayout is recognized as 5.1");
    expect(entry51.channelPositions.size() == 6,
           "5.1 entry has 6 channel positions serialized");

    expect(entry71.channels == 8, "7.1 entry channels is 8");
    expect(entry71.channelLayout != QStringLiteral("7.1") && entry71.channelPositions.isEmpty(),
           "Unsupported WAV 7.1 map is not falsely advertised as a known layout");

    {
        const auto json = entry71.toJson();
        const auto restored = abracapture::history::RecordingEntry::fromJson(json);
        expect(restored.channels == 8 && restored.channelLayout.isEmpty(),
               "Unknown WAV 7.1 layout round-trips through JSON without invention");
    }
}

void testRecordingSinkWavChannelLayoutPropagation() {
    QTemporaryDir dir;
    expect(dir.isValid(), "Temporary directory created for production layout propagation");
    if (!dir.isValid()) return;

    const std::filesystem::path indexPath = dir.path().toStdString() + "/history.json";
    abracapture::recording::OutputPathPolicy policy(
        dir.path().toStdString(), "production_master", "wav");
    abracapture::recording::CaptureMetadata metadata;
    metadata.source = "pipewire-test";
    metadata.format = "wav";
    metadata.channels = 6;
    metadata.sampleRate = 48000;

    const auto layout = abracapture::exporting::AudioChannelLayout::surround51();
    const AudioFormat format{
        .sampleRate = 48000,
        .channels = 6,
        .sampleFormat = SampleFormat::Int16,
        .channelLayout = layout,
    };

    abracapture::recording::RecordingSink sink;
    expect(sink.open(format, metadata, policy),
           "RecordingSink opens a known 5.1 WAV master");
    if (!sink.lastError().empty()) {
        std::cerr << "RecordingSink error: " << sink.lastError() << "\n";
    }
    const std::vector<float> samples(6 * 4, 0.0F);
    expect(sink.pushSamples(samples.data(), 4),
           "RecordingSink writes production 5.1 samples");
    const auto result = sink.finalize();
    expect(result.success, "RecordingSink finalizes the production 5.1 master");
    expect(result.channelLayout == layout,
           "CaptureResult retains the negotiated 5.1 layout");
    if (!result.success) return;

    SF_INFO info{};
    SNDFILE* const file = sf_open(result.finalPath.string().c_str(), SFM_READ, &info);
    expect(file != nullptr, "Production WAV master reopens through libsndfile");
    if (file != nullptr) {
        std::vector<int> channelMap(6, SF_CHANNEL_MAP_INVALID);
        const bool hasMap = sf_command(file, SFC_GET_CHANNEL_MAP_INFO, channelMap.data(),
                                       static_cast<int>(sizeof(int) * channelMap.size())) == SF_TRUE;
        expect(hasMap, "Reopened production WAV master exposes a channel map");
        if (hasMap) {
            expect(channelMap[0] == SF_CHANNEL_MAP_FRONT_LEFT || channelMap[0] == SF_CHANNEL_MAP_LEFT,
                   "Reopened WAV channel 0 is front left");
            expect(channelMap[1] == SF_CHANNEL_MAP_FRONT_RIGHT || channelMap[1] == SF_CHANNEL_MAP_RIGHT,
                   "Reopened WAV channel 1 is front right");
            expect(channelMap[2] == SF_CHANNEL_MAP_FRONT_CENTER || channelMap[2] == SF_CHANNEL_MAP_CENTER,
                   "Reopened WAV channel 2 is front center");
            expect(channelMap[3] == SF_CHANNEL_MAP_LFE &&
                       channelMap[4] == SF_CHANNEL_MAP_REAR_LEFT &&
                       channelMap[5] == SF_CHANNEL_MAP_REAR_RIGHT,
                   "Reopened WAV preserves LFE and rear speaker identities");
        }
        sf_close(file);
    }

    abracapture::history::RecordingIndex index(QString::fromStdString(indexPath.string()));
    expect(index.importDirectory(dir.path()),
           "RecordingIndex imports the production WAV master");
    expect(index.count() == 1, "RecordingIndex contains the production master");
    const auto entry = index.at(0);
    expect(entry.has_value(), "Production master index entry is available");
    if (!entry.has_value()) return;
    expect(entry->channelLayout == QStringLiteral("5.1") &&
               entry->channelPositions == layout.positionNames(),
           "RecordingIndex reconstructs the same 5.1 layout");

    abracapture::exporting::ExportRequest request;
    request.sourcePath = result.finalPath;
    request.sourceFormat.sampleRate = entry->sampleRate;
    request.sourceFormat.channelLayout =
        abracapture::exporting::AudioChannelLayout::fromPositionNames(
            entry->channelPositions, entry->channels);
    request.profile = abracapture::exporting::defaultExportProfile(
        abracapture::exporting::AudioContainer::Eac3,
        abracapture::exporting::AudioCodec::Eac3);
    expect(request.sourceFormat.channelLayout == layout,
           "ExportRequest receives the persisted production 5.1 layout");

    const AudioFormat unknownFormat{
        .sampleRate = 48000,
        .channels = 6,
        .sampleFormat = SampleFormat::Int16,
        .channelLayout = abracapture::exporting::AudioChannelLayout::unknown(6),
    };
    abracapture::recording::RecordingSink unknownSink;
    abracapture::recording::OutputPathPolicy unknownPolicy(
        dir.path().toStdString(), "unknown_master", "wav");
    const bool unknownOpened = unknownSink.open(unknownFormat, metadata, unknownPolicy);
    expect(!unknownOpened &&
               QString::fromStdString(unknownSink.lastError()).contains(QStringLiteral("unknown"), Qt::CaseInsensitive),
           "WAV recording refuses an unknown multichannel layout instead of inventing 5.1");
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    testAudioContainerRoundTrip();
    testAudioCodecRoundTrip();
    testAudioSampleFormatRoundTrip();
    testAudioChannelLayout();
    testAudioStreamFormat();
    testAudioEncodingTargetValidation();
    testAudioFormatCatalog();
    testFfmpegCapabilities();
    testPathSanitizer();
    testLegacyFormatConversion();
    testMp3Cbr320PreservesSourceRate();
    testMp3ExplicitSampleRate();
    testMp3RejectsUnsupportedPreserveSourceRate();
    testFlacOptions();
    testFlac16ExplicitRateUsesDitheredResample();
    testWavSampleFormat();
    testInvalidProfileIsRejected();
    testOutputPlanningAvoidsCollisions();
    testManualProfileModesAndBulkPreflight();
    testQueueAcceptsMultiplePendingJobs();
    testManualDirectoryEnumerationAndOutputPlanning();
    testQueuedJobCanBeCancelled();
    testCaptureSessionResolutionAndRetry();
    testManualFileWorkflowCompletes();
    testMetadataNaming();
    testManagerGeneratesMp3320();
    testDerivedExportSerialization();
    testNewFormatArgumentBuilders();
    testMultichannelSurroundValidation();
    testRealEncodingMatrix();
    testMultichannelWavPersistenceAndLayoutPropagation();
    testRecordingSinkWavChannelLayoutPropagation();
    return failures == 0 ? 0 : 1;
}
