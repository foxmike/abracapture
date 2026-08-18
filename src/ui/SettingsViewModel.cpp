#include "SettingsViewModel.hpp"
#include "config/AppSettings.hpp"
#include "recording/OutputPathPolicy.hpp"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

#include <cmath>

namespace {
constexpr double kDefaultAutoStartThreshold = -30.0;
constexpr int kDefaultAutoStopSilenceDuration = 3;

constexpr auto kAutomaticExportEnabledKey = "automaticExport/enabled";
constexpr auto kAutomaticExportFormatKey = "automaticExport/format";
constexpr auto kAutomaticExportSampleRatePolicyKey = "automaticExport/sampleRatePolicy";
constexpr auto kAutomaticExportSampleRateKey = "automaticExport/sampleRate";
constexpr auto kAutomaticExportKeepMasterKey = "automaticExport/keepMaster";
constexpr auto kAutomaticExportCollisionPolicyKey = "automaticExport/collisionPolicy";
constexpr auto kAutomaticExportMp3BitrateKey = "automaticExport/mp3/bitrateKbps";
constexpr auto kAutomaticExportMp3BitrateModeKey = "automaticExport/mp3/bitrateMode";
constexpr auto kAutomaticExportFlacBitDepthKey = "automaticExport/flac/bitDepth";
constexpr auto kAutomaticExportFlacCompressionKey = "automaticExport/flac/compressionLevel";
constexpr auto kAutomaticExportWavSampleFormatKey = "automaticExport/wav/sampleFormat";
constexpr auto kAutomaticExportOggQualityKey = "automaticExport/ogg/quality";
constexpr auto kAutomaticExportDestinationPolicyKey = "automaticExport/destinationPolicy";
constexpr auto kAutomaticExportCustomDirectoryKey = "automaticExport/customDirectory";

QString sampleRatePolicyName(const abracapture::exporting::SampleRatePolicy policy) {
    return policy == abracapture::exporting::SampleRatePolicy::Explicit
               ? QStringLiteral("explicit")
               : QStringLiteral("preserve_source");
}

QString collisionPolicyName(const abracapture::exporting::CollisionPolicy policy) {
    return policy == abracapture::exporting::CollisionPolicy::FailIfExists
               ? QStringLiteral("fail")
               : QStringLiteral("generate_unique");
}

QString wavSampleFormatName(const abracapture::exporting::WavSampleFormat sampleFormat) {
    switch (sampleFormat) {
    case abracapture::exporting::WavSampleFormat::Signed16: return QStringLiteral("s16");
    case abracapture::exporting::WavSampleFormat::Signed24: return QStringLiteral("s24");
    case abracapture::exporting::WavSampleFormat::Signed32: return QStringLiteral("s32");
    case abracapture::exporting::WavSampleFormat::Float32: return QStringLiteral("f32");
    case abracapture::exporting::WavSampleFormat::Unsigned8: return QStringLiteral("u8");
    }
    return QStringLiteral("s16");
}

std::optional<abracapture::exporting::WavSampleFormat> wavSampleFormatFromName(
    const QString& sampleFormat) {
    const QString normalized = sampleFormat.trimmed().toLower();
    if (normalized == QStringLiteral("s16")) return abracapture::exporting::WavSampleFormat::Signed16;
    if (normalized == QStringLiteral("s24")) return abracapture::exporting::WavSampleFormat::Signed24;
    if (normalized == QStringLiteral("s32")) return abracapture::exporting::WavSampleFormat::Signed32;
    if (normalized == QStringLiteral("f32")) return abracapture::exporting::WavSampleFormat::Float32;
    return std::nullopt;
}

QString destinationPolicyName(
    const abracapture::exporting::AutomaticExportDestinationPolicy policy) {
    switch (policy) {
    case abracapture::exporting::AutomaticExportDestinationPolicy::SameFolder:
        return QStringLiteral("same");
    case abracapture::exporting::AutomaticExportDestinationPolicy::ExportsSubfolder:
        return QStringLiteral("exports");
    case abracapture::exporting::AutomaticExportDestinationPolicy::CustomFolder:
        return QStringLiteral("custom");
    }
    return QStringLiteral("same");
}

abracapture::exporting::AutomaticExportDestinationPolicy destinationPolicyFromName(
    const QString& policy) {
    const QString normalized = policy.trimmed().toLower();
    if (normalized == QStringLiteral("exports")) {
        return abracapture::exporting::AutomaticExportDestinationPolicy::ExportsSubfolder;
    }
    if (normalized == QStringLiteral("custom")) {
        return abracapture::exporting::AutomaticExportDestinationPolicy::CustomFolder;
    }
    return abracapture::exporting::AutomaticExportDestinationPolicy::SameFolder;
}

void saveAutomaticExportProfile(
    QSettings& settings,
    const abracapture::exporting::AutomaticExportProfile& automaticProfile) {
    const auto& profile = automaticProfile.profile;
    settings.setValue(QString::fromUtf8(kAutomaticExportEnabledKey), automaticProfile.enabled);
    settings.setValue(QString::fromUtf8(kAutomaticExportFormatKey),
                      abracapture::exporting::exportFormatName(profile.format));
    settings.setValue(QString::fromUtf8(kAutomaticExportSampleRatePolicyKey),
                      sampleRatePolicyName(profile.sampleRate.policy));
    settings.setValue(QString::fromUtf8(kAutomaticExportSampleRateKey),
                      static_cast<int>(profile.sampleRate.sampleRate));
    settings.setValue(QString::fromUtf8(kAutomaticExportKeepMasterKey), automaticProfile.keepMaster);
    settings.setValue(QString::fromUtf8(kAutomaticExportCollisionPolicyKey),
                      collisionPolicyName(profile.collisionPolicy));
    settings.setValue(QString::fromUtf8(kAutomaticExportDestinationPolicyKey),
                      destinationPolicyName(automaticProfile.destinationPolicy));
    settings.setValue(QString::fromUtf8(kAutomaticExportCustomDirectoryKey),
                      automaticProfile.customDestinationDirectory);

    if (const auto* const options = std::get_if<abracapture::exporting::Mp3Options>(&profile.options)) {
        settings.setValue(QString::fromUtf8(kAutomaticExportMp3BitrateKey),
                          static_cast<int>(options->bitrateKbps));
        settings.setValue(QString::fromUtf8(kAutomaticExportMp3BitrateModeKey),
                          options->bitrateMode == abracapture::exporting::Mp3BitrateMode::Variable
                              ? QStringLiteral("vbr")
                              : QStringLiteral("cbr"));
    }
    if (const auto* const options = std::get_if<abracapture::exporting::FlacOptions>(&profile.options)) {
        settings.setValue(QString::fromUtf8(kAutomaticExportFlacBitDepthKey),
                          static_cast<int>(options->bitDepth));
        settings.setValue(QString::fromUtf8(kAutomaticExportFlacCompressionKey),
                          options->compressionLevel);
    }
    if (const auto* const options = std::get_if<abracapture::exporting::WavOptions>(&profile.options)) {
        settings.setValue(QString::fromUtf8(kAutomaticExportWavSampleFormatKey),
                          static_cast<int>(options->sampleFormat));
    }
    if (const auto* const options = std::get_if<abracapture::exporting::OggVorbisOptions>(&profile.options)) {
        settings.setValue(QString::fromUtf8(kAutomaticExportOggQualityKey), options->quality);
    }
}
} // namespace

SettingsViewModel::SettingsViewModel(QObject* parent)
    : QObject(parent) {
    load();
}

QString SettingsViewModel::outputDirectory() const {
    return outputDirectory_;
}

void SettingsViewModel::setOutputDirectory(const QString& outputDirectory) {
    if (outputDirectory_ == outputDirectory) {
        return;
    }
    outputDirectory_ = outputDirectory;
    emit outputDirectoryChanged(outputDirectory_);
    validate();
}

QString SettingsViewModel::namingPattern() const {
    return namingPattern_;
}

void SettingsViewModel::setNamingPattern(const QString& namingPattern) {
    if (namingPattern_ == namingPattern) {
        return;
    }
    namingPattern_ = namingPattern;
    emit namingPatternChanged(namingPattern_);
    emit fileNamingPatternChanged(namingPattern_);
    validate();
}

QString SettingsViewModel::masterFormat() const {
    return masterFormat_;
}

int SettingsViewModel::captureSampleRate() const noexcept { return captureSampleRate_; }

void SettingsViewModel::setCaptureSampleRate(const int sampleRate) {
    if (sampleRate != 0 && sampleRate != 48000 && sampleRate != 96000 && sampleRate != 128000 && sampleRate != 192000) {
        return;
    }
    if (captureSampleRate_ == sampleRate) return;
    captureSampleRate_ = sampleRate;
    emit captureSampleRateChanged(captureSampleRate_);
}

int SettingsViewModel::captureBitDepth() const noexcept { return captureBitDepth_; }

void SettingsViewModel::setCaptureBitDepth(const int bitDepth) {
    if (bitDepth != 0 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32) return;
    if (captureBitDepth_ == bitDepth) return;
    captureBitDepth_ = bitDepth;
    emit captureBitDepthChanged(captureBitDepth_);
}

void SettingsViewModel::setMasterFormat(const QString& masterFormat) {
    const QString normalized = masterFormat.trimmed().toLower();
    if (normalized != QStringLiteral("wav") && normalized != QStringLiteral("flac")) {
        return;
    }
    if (masterFormat_ == normalized) {
        return;
    }
    masterFormat_ = normalized;
    emit masterFormatChanged(masterFormat_);
    validate();
}

bool SettingsViewModel::exportEnabled() const noexcept {
    return automaticExportProfile_.enabled;
}

void SettingsViewModel::setExportEnabled(bool exportEnabled) {
    if (automaticExportProfile_.enabled == exportEnabled) {
        return;
    }
    automaticExportProfile_.enabled = exportEnabled;
    emit exportEnabledChanged(automaticExportProfile_.enabled);
    emit automaticExportProfileChanged();
    save();
}

QString SettingsViewModel::exportFormat() const {
    return abracapture::exporting::exportFormatName(automaticExportProfile_.profile.format);
}

void SettingsViewModel::setExportFormat(const QString& exportFormat) {
    const auto parsedFormat = abracapture::exporting::exportFormatFromString(exportFormat);
    if (!parsedFormat.has_value()) {
        return;
    }
    if (automaticExportProfile_.profile.format == *parsedFormat) {
        return;
    }
    automaticExportProfile_.profile = abracapture::exporting::defaultExportProfile(*parsedFormat);
    emit exportFormatChanged(this->exportFormat());
    emit automaticExportProfileChanged();
    save();
}

abracapture::exporting::AutomaticExportProfile SettingsViewModel::automaticExportProfile() const {
    return automaticExportProfile_;
}

void SettingsViewModel::setAutomaticExportProfile(
    const abracapture::exporting::AutomaticExportProfile& profile) {
    QString error;
    if (!profile.profile.validate(&error)) {
        return;
    }
    if (profile.destinationPolicy ==
            abracapture::exporting::AutomaticExportDestinationPolicy::CustomFolder &&
        profile.customDestinationDirectory.trimmed().isEmpty()) {
        return;
    }
    const QString previousFormat = exportFormat();
    const bool previousEnabled = exportEnabled();
    automaticExportProfile_ = profile;
    if (previousEnabled != exportEnabled()) {
        emit exportEnabledChanged(exportEnabled());
    }
    if (previousFormat != exportFormat()) {
        emit exportFormatChanged(exportFormat());
    }
    emit automaticExportProfileChanged();
    save();
}

int SettingsViewModel::automaticExportSampleRate() const noexcept {
    return automaticExportProfile_.profile.sampleRate.policy ==
                   abracapture::exporting::SampleRatePolicy::Explicit
               ? static_cast<int>(automaticExportProfile_.profile.sampleRate.sampleRate)
               : 0;
}

void SettingsViewModel::setAutomaticExportSampleRate(const int sampleRate) {
    auto profile = automaticExportProfile_;
    if (sampleRate == 0) {
        profile.profile.sampleRate = {abracapture::exporting::SampleRatePolicy::PreserveSource, 0};
    } else {
        if (!abracapture::exporting::supportsExplicitSampleRate(
                profile.profile.format, static_cast<std::uint32_t>(sampleRate))) {
            return;
        }
        profile.profile.sampleRate = {abracapture::exporting::SampleRatePolicy::Explicit,
                                      static_cast<std::uint32_t>(sampleRate)};
    }
    setAutomaticExportProfile(profile);
}

int SettingsViewModel::automaticExportMp3Bitrate() const noexcept {
    if (const auto* const options = std::get_if<abracapture::exporting::Mp3Options>(
            &automaticExportProfile_.profile.options)) {
        return static_cast<int>(options->bitrateKbps);
    }
    return 320;
}

void SettingsViewModel::setAutomaticExportMp3Bitrate(const int bitrateKbps) {
    if (automaticExportProfile_.profile.format != abracapture::exporting::ExportFormat::Mp3) {
        return;
    }
    auto profile = automaticExportProfile_;
    profile.profile.options = abracapture::exporting::Mp3Options{
        static_cast<std::uint32_t>(bitrateKbps), abracapture::exporting::Mp3BitrateMode::Constant};
    setAutomaticExportProfile(profile);
}

int SettingsViewModel::automaticExportFlacBitDepth() const noexcept {
    if (const auto* const options = std::get_if<abracapture::exporting::FlacOptions>(
            &automaticExportProfile_.profile.options)) {
        return static_cast<int>(options->bitDepth);
    }
    return 16;
}

void SettingsViewModel::setAutomaticExportFlacBitDepth(const int bitDepth) {
    if (automaticExportProfile_.profile.format != abracapture::exporting::ExportFormat::Flac) {
        return;
    }
    auto profile = automaticExportProfile_;
    const auto* const current = std::get_if<abracapture::exporting::FlacOptions>(
        &profile.profile.options);
    profile.profile.options = abracapture::exporting::FlacOptions{
        static_cast<std::uint32_t>(bitDepth), current != nullptr ? current->compressionLevel : 5};
    setAutomaticExportProfile(profile);
}

int SettingsViewModel::automaticExportFlacCompressionLevel() const noexcept {
    if (const auto* const options = std::get_if<abracapture::exporting::FlacOptions>(
            &automaticExportProfile_.profile.options)) {
        return options->compressionLevel;
    }
    return 5;
}

void SettingsViewModel::setAutomaticExportFlacCompressionLevel(const int compressionLevel) {
    if (automaticExportProfile_.profile.format != abracapture::exporting::ExportFormat::Flac) {
        return;
    }
    auto profile = automaticExportProfile_;
    const auto* const current = std::get_if<abracapture::exporting::FlacOptions>(
        &profile.profile.options);
    profile.profile.options = abracapture::exporting::FlacOptions{
        current != nullptr ? current->bitDepth : 16, compressionLevel};
    setAutomaticExportProfile(profile);
}

QString SettingsViewModel::automaticExportWavSampleFormat() const {
    if (const auto* const options = std::get_if<abracapture::exporting::WavOptions>(
            &automaticExportProfile_.profile.options)) {
        return wavSampleFormatName(options->sampleFormat);
    }
    return QStringLiteral("s16");
}

void SettingsViewModel::setAutomaticExportWavSampleFormat(const QString& sampleFormat) {
    if (automaticExportProfile_.profile.format != abracapture::exporting::ExportFormat::Wav) {
        return;
    }
    const auto parsed = wavSampleFormatFromName(sampleFormat);
    if (!parsed.has_value()) {
        return;
    }
    auto profile = automaticExportProfile_;
    profile.profile.options = abracapture::exporting::WavOptions{*parsed};
    setAutomaticExportProfile(profile);
}

QStringList SettingsViewModel::automaticExportFormats() const {
    return {QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("wav")};
}

QStringList SettingsViewModel::automaticExportMp3BitrateOptions() const {
    return {QStringLiteral("128"), QStringLiteral("192"), QStringLiteral("256"),
            QStringLiteral("320")};
}

QStringList SettingsViewModel::automaticExportFlacBitDepthOptions() const {
    return {QStringLiteral("16"), QStringLiteral("24"), QStringLiteral("32")};
}

QStringList SettingsViewModel::automaticExportFlacCompressionOptions() const {
    QStringList options;
    for (int level = 0; level <= 12; ++level) {
        options.append(QString::number(level));
    }
    return options;
}

QStringList SettingsViewModel::automaticExportWavSampleFormatOptions() const {
    return {QStringLiteral("s16"), QStringLiteral("s24"), QStringLiteral("s32"),
            QStringLiteral("f32")};
}

QStringList SettingsViewModel::automaticExportSampleRateOptions() const {
    if (automaticExportProfile_.profile.format == abracapture::exporting::ExportFormat::Mp3 ||
        automaticExportProfile_.profile.format == abracapture::exporting::ExportFormat::OggVorbis) {
        return {QStringLiteral("preserve"), QStringLiteral("44100"), QStringLiteral("48000")};
    }
    return {QStringLiteral("preserve"), QStringLiteral("44100"), QStringLiteral("48000"),
            QStringLiteral("88200"), QStringLiteral("96000"), QStringLiteral("176400"),
            QStringLiteral("192000")};
}

QString SettingsViewModel::automaticExportDestinationPolicy() const {
    return destinationPolicyName(automaticExportProfile_.destinationPolicy);
}

void SettingsViewModel::setAutomaticExportDestinationPolicy(const QString& policy) {
    auto profile = automaticExportProfile_;
    profile.destinationPolicy = destinationPolicyFromName(policy);
    if (profile.destinationPolicy ==
            abracapture::exporting::AutomaticExportDestinationPolicy::CustomFolder &&
        profile.customDestinationDirectory.trimmed().isEmpty()) {
        return;
    }
    setAutomaticExportProfile(profile);
}

QString SettingsViewModel::automaticExportCustomDirectory() const {
    return automaticExportProfile_.customDestinationDirectory;
}

void SettingsViewModel::setAutomaticExportCustomDirectory(const QString& directory) {
    auto profile = automaticExportProfile_;
    profile.customDestinationDirectory = directory.trimmed();
    if (profile.destinationPolicy ==
            abracapture::exporting::AutomaticExportDestinationPolicy::CustomFolder &&
        profile.customDestinationDirectory.isEmpty()) {
        return;
    }
    setAutomaticExportProfile(profile);
}

QString SettingsViewModel::automaticExportPreset() const {
    if (automaticExportProfile_.profile.sampleRate.policy ==
            abracapture::exporting::SampleRatePolicy::PreserveSource &&
        automaticExportProfile_.profile.format == abracapture::exporting::ExportFormat::Mp3) {
        if (const auto* const options = std::get_if<abracapture::exporting::Mp3Options>(
                &automaticExportProfile_.profile.options);
            options != nullptr && options->bitrateKbps == 320 &&
            options->bitrateMode == abracapture::exporting::Mp3BitrateMode::Constant) {
            return QStringLiteral("mp3_320");
        }
    }
    if (automaticExportProfile_.profile.sampleRate.policy ==
            abracapture::exporting::SampleRatePolicy::PreserveSource &&
        automaticExportProfile_.profile.format == abracapture::exporting::ExportFormat::Mp3) {
        if (const auto* const options = std::get_if<abracapture::exporting::Mp3Options>(
                &automaticExportProfile_.profile.options);
            options != nullptr && options->bitrateKbps == 256 &&
            options->bitrateMode == abracapture::exporting::Mp3BitrateMode::Constant) {
            return QStringLiteral("mp3_256");
        }
    }
    if (automaticExportProfile_.profile.sampleRate.policy ==
            abracapture::exporting::SampleRatePolicy::PreserveSource &&
        automaticExportProfile_.profile.format == abracapture::exporting::ExportFormat::Flac) {
        if (const auto* const options = std::get_if<abracapture::exporting::FlacOptions>(
                &automaticExportProfile_.profile.options);
            options != nullptr && options->compressionLevel == 5 && options->bitDepth == 16) {
            return QStringLiteral("flac_16");
        }
        if (const auto* const options = std::get_if<abracapture::exporting::FlacOptions>(
                &automaticExportProfile_.profile.options);
            options != nullptr && options->compressionLevel == 5 && options->bitDepth == 24) {
            return QStringLiteral("flac_24");
        }
    }
    if (automaticExportProfile_.profile.sampleRate.policy ==
            abracapture::exporting::SampleRatePolicy::PreserveSource &&
        automaticExportProfile_.profile.format == abracapture::exporting::ExportFormat::Wav) {
        if (const auto* const options = std::get_if<abracapture::exporting::WavOptions>(
                &automaticExportProfile_.profile.options);
            options != nullptr && options->sampleFormat ==
                abracapture::exporting::WavSampleFormat::Signed24) {
            return QStringLiteral("wav_24");
        }
    }
    return QStringLiteral("custom");
}

void SettingsViewModel::setAutomaticExportPreset(const QString& preset) {
    const QString normalized = preset.trimmed().toLower();
    if (normalized == QStringLiteral("custom")) {
        return;
    }
    auto profile = automaticExportProfile_;
    if (normalized == QStringLiteral("mp3_320")) {
        profile.profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::ExportFormat::Mp3);
    } else if (normalized == QStringLiteral("mp3_256")) {
        profile.profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::ExportFormat::Mp3);
        profile.profile.options = abracapture::exporting::Mp3Options{256,
            abracapture::exporting::Mp3BitrateMode::Constant};
    } else if (normalized == QStringLiteral("flac_16")) {
        profile.profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::ExportFormat::Flac);
        profile.profile.options = abracapture::exporting::FlacOptions{16, 5};
    } else if (normalized == QStringLiteral("flac_24")) {
        profile.profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::ExportFormat::Flac);
        profile.profile.options = abracapture::exporting::FlacOptions{24, 5};
    } else if (normalized == QStringLiteral("wav_24")) {
        profile.profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::ExportFormat::Wav);
        profile.profile.options = abracapture::exporting::WavOptions{
            abracapture::exporting::WavSampleFormat::Signed24};
    } else if (normalized == QStringLiteral("preserve_lossless")) {
        profile.profile = abracapture::exporting::defaultExportProfile(
            abracapture::exporting::ExportFormat::Wav);
        profile.profile.options = abracapture::exporting::WavOptions{
            abracapture::exporting::WavSampleFormat::Float32};
    } else {
        return;
    }
    setAutomaticExportProfile(profile);
}

QString SettingsViewModel::automaticExportSummary() const {
    const auto& profile = automaticExportProfile_.profile;
    QString summary = abracapture::exporting::exportFormatName(profile.format).toUpper();
    if (profile.sampleRate.policy == abracapture::exporting::SampleRatePolicy::PreserveSource) {
        summary += QStringLiteral(" • Preserve source rate");
    } else {
        summary += QStringLiteral(" • %1 kHz")
                       .arg(static_cast<double>(profile.sampleRate.sampleRate) / 1000.0, 0, 'f',
                            profile.sampleRate.sampleRate % 1000 == 0 ? 0 : 1);
    }
    if (const auto* const options = std::get_if<abracapture::exporting::Mp3Options>(
            &profile.options)) {
        summary += QStringLiteral(" • %1 kbps").arg(options->bitrateKbps);
    } else if (const auto* const options = std::get_if<abracapture::exporting::FlacOptions>(
                   &profile.options)) {
        summary += QStringLiteral(" • %1-bit").arg(options->bitDepth);
    } else if (const auto* const options = std::get_if<abracapture::exporting::WavOptions>(
                   &profile.options)) {
        summary += QStringLiteral(" • %1").arg(wavSampleFormatName(options->sampleFormat));
    }
    return summary;
}

QString SettingsViewModel::automaticExportPreviewPath(const QVariantMap& source) const {
    abracapture::recording::CaptureMetadata metadata;
    metadata.source = source.value(QStringLiteral("displayName"),
                                   source.value(QStringLiteral("appName"))).toString().toStdString();
    metadata.sampleRate = source.value(QStringLiteral("sampleRate")).toUInt();
    metadata.channels = static_cast<std::uint16_t>(source.value(QStringLiteral("channelCount")).toUInt());
    metadata.format = masterFormat_.toStdString();
    metadata.mediaTitle = source.value(QStringLiteral("mediaTitle")).toString().toStdString();
    metadata.mediaName = source.value(QStringLiteral("mediaName")).toString().toStdString();
    metadata.mediaAlbum = source.value(QStringLiteral("mediaAlbum")).toString().toStdString();
    metadata.applicationName = source.value(QStringLiteral("appName")).toString().toStdString();
    metadata.automatedCapture = true;
    metadata.useMediaMetadataNames = useMediaMetadataForAutomatedCaptureNames_;
    metadata.createDirectories = false;

    abracapture::recording::OutputPathPolicy pathPolicy(
        outputDirectory_.toStdString(), namingPattern_.toStdString(), masterFormat_.toStdString());
    const std::filesystem::path masterPath = pathPolicy.resolveOutputPath(metadata);
    std::filesystem::path destinationDirectory = masterPath.parent_path();
    if (automaticExportProfile_.destinationPolicy ==
        abracapture::exporting::AutomaticExportDestinationPolicy::ExportsSubfolder) {
        destinationDirectory /= "Exports";
    } else if (automaticExportProfile_.destinationPolicy ==
               abracapture::exporting::AutomaticExportDestinationPolicy::CustomFolder) {
        destinationDirectory = automaticExportProfile_.customDestinationDirectory.toStdString();
    }
    std::filesystem::path exportPath = destinationDirectory / masterPath.filename();
    exportPath.replace_extension(
        "." + abracapture::exporting::exportFileExtension(
                   automaticExportProfile_.profile.format).toStdString());
    return QDir::toNativeSeparators(QString::fromStdString(exportPath.string()));
}

bool SettingsViewModel::autoStartEnabled() const noexcept {
    return autoStartEnabled_;
}

void SettingsViewModel::setAutoStartEnabled(bool autoStartEnabled) {
    if (autoStartEnabled_ == autoStartEnabled) {
        return;
    }
    autoStartEnabled_ = autoStartEnabled;
    emit autoStartEnabledChanged(autoStartEnabled_);
    save();
}

double SettingsViewModel::autoStartThreshold() const noexcept {
    return autoStartThreshold_;
}

void SettingsViewModel::setAutoStartThreshold(double autoStartThreshold) {
    if (autoStartThreshold > 0.0 || autoStartThreshold < -120.0 ||
        std::isnan(autoStartThreshold)) {
        return;
    }
    if (qFuzzyCompare(autoStartThreshold_, autoStartThreshold)) {
        return;
    }
    autoStartThreshold_ = autoStartThreshold;
    emit autoStartThresholdChanged(autoStartThreshold_);
    save();
}

bool SettingsViewModel::autoStopEnabled() const noexcept {
    return autoStopEnabled_;
}

void SettingsViewModel::setAutoStopEnabled(bool autoStopEnabled) {
    if (autoStopEnabled_ == autoStopEnabled) {
        return;
    }
    autoStopEnabled_ = autoStopEnabled;
    emit autoStopEnabledChanged(autoStopEnabled_);
    save();
}

int SettingsViewModel::autoStopSilenceDuration() const noexcept {
    return autoStopSilenceDuration_;
}

void SettingsViewModel::setAutoStopSilenceDuration(int autoStopSilenceDuration) {
    if (autoStopSilenceDuration < 1 || autoStopSilenceDuration > 300) {
        return;
    }
    if (autoStopSilenceDuration_ == autoStopSilenceDuration) {
        return;
    }
    autoStopSilenceDuration_ = autoStopSilenceDuration;
    emit autoStopSilenceDurationChanged(autoStopSilenceDuration_);
    save();
}

int SettingsViewModel::preRollSeconds() const noexcept {
    return preRollSeconds_;
}

void SettingsViewModel::setPreRollSeconds(int preRollSeconds) {
    if (preRollSeconds < 0 || preRollSeconds > 300) {
        return;
    }
    if (preRollSeconds_ == preRollSeconds) {
        return;
    }
    preRollSeconds_ = preRollSeconds;
    emit preRollSecondsChanged(preRollSeconds_);
    save();
}

int SettingsViewModel::postRollSeconds() const noexcept {
    return postRollSeconds_;
}

bool SettingsViewModel::useMediaMetadataForAutomatedCaptureNames() const noexcept {
    return useMediaMetadataForAutomatedCaptureNames_;
}

void SettingsViewModel::setUseMediaMetadataForAutomatedCaptureNames(const bool enabled) {
    if (useMediaMetadataForAutomatedCaptureNames_ == enabled) {
        return;
    }
    useMediaMetadataForAutomatedCaptureNames_ = enabled;
    emit useMediaMetadataForAutomatedCaptureNamesChanged(
        useMediaMetadataForAutomatedCaptureNames_);
    save();
}

void SettingsViewModel::setPostRollSeconds(int postRollSeconds) {
    if (postRollSeconds < 0 || postRollSeconds > 300) {
        return;
    }
    if (postRollSeconds_ == postRollSeconds) {
        return;
    }
    postRollSeconds_ = postRollSeconds;
    emit postRollSecondsChanged(postRollSeconds_);
    save();
}

bool SettingsViewModel::isValid() const noexcept {
    return isValid_;
}

QString SettingsViewModel::errorMessage() const {
    return errorMessage_;
}

QStringList SettingsViewModel::availableMasterFormats() const {
    return {QStringLiteral("wav"), QStringLiteral("flac")};
}

QStringList SettingsViewModel::availableExportFormats() const {
    return {QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"), QStringLiteral("wav")};
}

QStringList SettingsViewModel::availableSampleRates() const {
    return {QStringLiteral("Auto (source)"), QStringLiteral("48000"), QStringLiteral("96000"), QStringLiteral("128000"), QStringLiteral("192000")};
}

QStringList SettingsViewModel::availableBitDepths() const {
    return {QStringLiteral("Auto (source)"), QStringLiteral("16"), QStringLiteral("24"), QStringLiteral("32")};
}

void SettingsViewModel::setErrorMessage(const QString& message) {
    if (errorMessage_ == message) {
        return;
    }
    errorMessage_ = message;
    emit errorMessageChanged(errorMessage_);
}

void SettingsViewModel::setIsValid(bool valid) {
    if (isValid_ == valid) {
        return;
    }
    isValid_ = valid;
    emit validityChanged(isValid_);
}

void SettingsViewModel::load() {
    AppSettings appSettings;
    appSettings.load();

    QSettings settings(QStringLiteral("Abracapture"), QStringLiteral("Abracapture"));

    const QString outDir = appSettings.outputDirectory();
    const QString pattern = appSettings.fileNamingPattern();
    const QString mFormat = appSettings.masterFormat();
    const int sampleRate = settings.value(QStringLiteral("captureSampleRate"), 0).toInt();
    const int bitDepth = settings.value(QStringLiteral("captureBitDepth"), 0).toInt();
    const bool autoStart = appSettings.autoStartEnabled();
    const bool autoStop = appSettings.autoStopEnabled();
    const int preRoll = appSettings.preRollSeconds();
    const int postRoll = appSettings.postRollSeconds();
    const bool useMediaMetadata = appSettings.useMediaMetadataForAutomatedCaptureNames();

    const bool hasAutomaticProfile =
        settings.contains(QString::fromUtf8(kAutomaticExportFormatKey));
    const bool legacyExportEnabled = settings.value(QStringLiteral("exportEnabled"), false).toBool();
    const QString legacyExportFormat =
        settings.value(QStringLiteral("exportFormat"), QStringLiteral("mp3")).toString();
    const QString configuredExportFormat = settings.value(
        QString::fromUtf8(kAutomaticExportFormatKey), legacyExportFormat).toString();
    const auto parsedExportFormat = abracapture::exporting::exportFormatFromString(configuredExportFormat);
    const auto configuredFormat = parsedExportFormat.value_or(abracapture::exporting::ExportFormat::Mp3);
    auto automaticProfile = abracapture::exporting::AutomaticExportProfile{};
    automaticProfile.enabled = settings.value(
        QString::fromUtf8(kAutomaticExportEnabledKey), legacyExportEnabled).toBool();
    automaticProfile.keepMaster = settings.value(
        QString::fromUtf8(kAutomaticExportKeepMasterKey), true).toBool();
    automaticProfile.destinationPolicy = destinationPolicyFromName(settings.value(
        QString::fromUtf8(kAutomaticExportDestinationPolicyKey), QStringLiteral("same")).toString());
    automaticProfile.customDestinationDirectory = settings.value(
        QString::fromUtf8(kAutomaticExportCustomDirectoryKey)).toString();
    if (automaticProfile.destinationPolicy ==
            abracapture::exporting::AutomaticExportDestinationPolicy::CustomFolder &&
        automaticProfile.customDestinationDirectory.trimmed().isEmpty()) {
        automaticProfile.destinationPolicy =
            abracapture::exporting::AutomaticExportDestinationPolicy::SameFolder;
    }
    automaticProfile.profile = abracapture::exporting::defaultExportProfile(configuredFormat);
    const QString sampleRatePolicy = settings.value(
        QString::fromUtf8(kAutomaticExportSampleRatePolicyKey), QStringLiteral("preserve_source")).toString();
    if (sampleRatePolicy.trimmed().toLower() == QStringLiteral("explicit")) {
        automaticProfile.profile.sampleRate = {
            abracapture::exporting::SampleRatePolicy::Explicit,
            static_cast<std::uint32_t>(settings.value(
                QString::fromUtf8(kAutomaticExportSampleRateKey), 0).toUInt())};
    }
    if (settings.value(QString::fromUtf8(kAutomaticExportCollisionPolicyKey),
                      QStringLiteral("generate_unique")).toString().trimmed().toLower() == QStringLiteral("fail")) {
        automaticProfile.profile.collisionPolicy =
            abracapture::exporting::CollisionPolicy::FailIfExists;
    }
    switch (configuredFormat) {
    case abracapture::exporting::ExportFormat::Mp3:
        automaticProfile.profile.options = abracapture::exporting::Mp3Options{
            static_cast<std::uint32_t>(settings.value(
                QString::fromUtf8(kAutomaticExportMp3BitrateKey), 320).toUInt()),
            settings.value(QString::fromUtf8(kAutomaticExportMp3BitrateModeKey),
                           QStringLiteral("cbr")).toString().trimmed().toLower() == QStringLiteral("vbr")
                ? abracapture::exporting::Mp3BitrateMode::Variable
                : abracapture::exporting::Mp3BitrateMode::Constant};
        break;
    case abracapture::exporting::ExportFormat::Flac:
        automaticProfile.profile.options = abracapture::exporting::FlacOptions{
            static_cast<std::uint32_t>(settings.value(
                QString::fromUtf8(kAutomaticExportFlacBitDepthKey), 16).toUInt()),
            settings.value(QString::fromUtf8(kAutomaticExportFlacCompressionKey), 5).toInt()};
        break;
    case abracapture::exporting::ExportFormat::Wav:
        automaticProfile.profile.options = abracapture::exporting::WavOptions{
            static_cast<abracapture::exporting::WavSampleFormat>(settings.value(
                QString::fromUtf8(kAutomaticExportWavSampleFormatKey),
                static_cast<int>(abracapture::exporting::WavSampleFormat::Signed16)).toInt())};
        break;
    case abracapture::exporting::ExportFormat::OggVorbis:
        automaticProfile.profile.options = abracapture::exporting::OggVorbisOptions{
            settings.value(QString::fromUtf8(kAutomaticExportOggQualityKey), 5).toInt()};
        break;
    }
    QString automaticProfileError;
    if (!automaticProfile.profile.validate(&automaticProfileError)) {
        automaticProfile.profile = abracapture::exporting::defaultExportProfile(configuredFormat);
    }
    const double autoStartThresh = settings.value(QStringLiteral("autoStartThreshold"), kDefaultAutoStartThreshold).toDouble();
    int autoStopDuration = settings.value(QStringLiteral("autoStopSilenceDuration"), kDefaultAutoStopSilenceDuration).toInt();
    if (autoStopDuration < 1 || autoStopDuration > 300) {
        autoStopDuration = kDefaultAutoStopSilenceDuration;
    }

    if (outputDirectory_ != outDir) {
        outputDirectory_ = outDir;
        emit outputDirectoryChanged(outputDirectory_);
    }
    if (namingPattern_ != pattern) {
        namingPattern_ = pattern;
        emit namingPatternChanged(namingPattern_);
        emit fileNamingPatternChanged(namingPattern_);
    }
    if (masterFormat_ != mFormat) {
        masterFormat_ = mFormat;
        emit masterFormatChanged(masterFormat_);
    }
    setCaptureSampleRate(sampleRate == 48000 || sampleRate == 96000 || sampleRate == 128000 || sampleRate == 192000 ? sampleRate : 0);
    setCaptureBitDepth(bitDepth == 16 || bitDepth == 24 || bitDepth == 32 ? bitDepth : 0);
    if (autoStartEnabled_ != autoStart) {
        autoStartEnabled_ = autoStart;
        emit autoStartEnabledChanged(autoStartEnabled_);
    }
    if (autoStopEnabled_ != autoStop) {
        autoStopEnabled_ = autoStop;
        emit autoStopEnabledChanged(autoStopEnabled_);
    }
    if (preRollSeconds_ != preRoll) {
        preRollSeconds_ = preRoll;
        emit preRollSecondsChanged(preRollSeconds_);
    }
    if (postRollSeconds_ != postRoll) {
        postRollSeconds_ = postRoll;
        emit postRollSecondsChanged(postRollSeconds_);
    }
    if (useMediaMetadataForAutomatedCaptureNames_ != useMediaMetadata) {
        useMediaMetadataForAutomatedCaptureNames_ = useMediaMetadata;
        emit useMediaMetadataForAutomatedCaptureNamesChanged(
            useMediaMetadataForAutomatedCaptureNames_);
    }
    const bool previousExportEnabled = exportEnabled();
    const QString previousExportFormat = exportFormat();
    automaticExportProfile_ = automaticProfile;
    emit automaticExportProfileChanged();
    if (previousExportEnabled != exportEnabled()) {
        emit exportEnabledChanged(exportEnabled());
    }
    if (previousExportFormat != exportFormat()) {
        emit exportFormatChanged(exportFormat());
    }
    if (!qFuzzyCompare(autoStartThreshold_, autoStartThresh)) {
        autoStartThreshold_ = autoStartThresh;
        emit autoStartThresholdChanged(autoStartThreshold_);
    }
    if (autoStopSilenceDuration_ != autoStopDuration) {
        autoStopSilenceDuration_ = autoStopDuration;
        emit autoStopSilenceDurationChanged(autoStopSilenceDuration_);
    }

    if (!hasAutomaticProfile) {
        saveAutomaticExportProfile(settings, automaticExportProfile_);
        settings.remove(QStringLiteral("exportEnabled"));
        settings.remove(QStringLiteral("exportFormat"));
        settings.sync();
    }

    validate();
}

void SettingsViewModel::save() {
    if (!validate()) {
        return;
    }

    AppSettings appSettings;
    appSettings.setOutputDirectory(outputDirectory_);
    appSettings.setFileNamingPattern(namingPattern_);
    appSettings.setMasterFormat(masterFormat_);
    appSettings.setAutoStartEnabled(autoStartEnabled_);
    appSettings.setAutoStopEnabled(autoStopEnabled_);
    appSettings.setPreRollSeconds(preRollSeconds_);
    appSettings.setPostRollSeconds(postRollSeconds_);
    appSettings.setUseMediaMetadataForAutomatedCaptureNames(
        useMediaMetadataForAutomatedCaptureNames_);
    appSettings.save();

    QSettings settings(QStringLiteral("Abracapture"), QStringLiteral("Abracapture"));
    saveAutomaticExportProfile(settings, automaticExportProfile_);
    settings.remove(QStringLiteral("exportEnabled"));
    settings.remove(QStringLiteral("exportFormat"));
    settings.setValue(QStringLiteral("captureSampleRate"), captureSampleRate_);
    settings.setValue(QStringLiteral("captureBitDepth"), captureBitDepth_);
    settings.setValue(QStringLiteral("autoStartThreshold"), autoStartThreshold_);
    settings.setValue(QStringLiteral("autoStopSilenceDuration"), autoStopSilenceDuration_);
    settings.sync();

    emit saved();
}

void SettingsViewModel::resetToDefaults() {
    setOutputDirectory(AppSettings::defaultOutputDirectory());
    setNamingPattern(AppSettings::defaultFileNamingPattern());
    setMasterFormat(AppSettings::defaultMasterFormat());
    setCaptureSampleRate(0);
    setCaptureBitDepth(0);
    setAutoStartEnabled(AppSettings::defaultAutoStartEnabled());
    setAutoStopEnabled(AppSettings::defaultAutoStopEnabled());
    setPreRollSeconds(AppSettings::defaultPreRollSeconds());
    setPostRollSeconds(AppSettings::defaultPostRollSeconds());
    setUseMediaMetadataForAutomatedCaptureNames(
        AppSettings::defaultUseMediaMetadataForAutomatedCaptureNames());
    setAutomaticExportProfile(abracapture::exporting::AutomaticExportProfile{});
    setAutoStartThreshold(kDefaultAutoStartThreshold);
    setAutoStopSilenceDuration(kDefaultAutoStopSilenceDuration);
    validate();
}

bool SettingsViewModel::validate() {
    const QString trimmedDir = outputDirectory_.trimmed();
    if (trimmedDir.isEmpty()) {
        const QString err = QStringLiteral("Output directory cannot be empty.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    const QString trimmedPattern = namingPattern_.trimmed();
    if (trimmedPattern.isEmpty()) {
        const QString err = QStringLiteral("File naming pattern cannot be empty.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    static const QRegularExpression invalidCharsRegex(QStringLiteral(R"([\\/:*?"<>|])"));
    if (trimmedPattern.contains(invalidCharsRegex)) {
        const QString err = QStringLiteral("File naming pattern contains invalid path characters.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    const QString normMaster = masterFormat_.trimmed().toLower();
    if (normMaster != QStringLiteral("wav") && normMaster != QStringLiteral("flac")) {
        const QString err = QStringLiteral("Master format must be WAV or FLAC.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    if (preRollSeconds_ < 0 || preRollSeconds_ > 300) {
        const QString err = QStringLiteral("Pre-roll duration must be between 0 and 300 seconds.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    if (postRollSeconds_ < 0 || postRollSeconds_ > 300) {
        const QString err = QStringLiteral("Post-roll duration must be between 0 and 300 seconds.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    if (autoStartThreshold_ > 0.0 || autoStartThreshold_ < -120.0) {
        const QString err = QStringLiteral("Auto-start threshold must be between -120 dBFS and 0 dBFS.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    if (autoStopSilenceDuration_ < 1 || autoStopSilenceDuration_ > 300) {
        const QString err = QStringLiteral("Auto-stop silence duration must be between 1 and 300 seconds.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    if (automaticExportProfile_.enabled) {
        QString profileError;
        if (!automaticExportProfile_.profile.validate(&profileError)) {
            const QString err = QStringLiteral("Automatic export profile is invalid: %1")
                                    .arg(profileError);
            setErrorMessage(err);
            setIsValid(false);
            emit errorOccurred(err);
            return false;
        }
    }

    if (captureSampleRate_ != 0 && captureSampleRate_ != 48000 && captureSampleRate_ != 96000 && captureSampleRate_ != 128000 && captureSampleRate_ != 192000) {
        const QString err = QStringLiteral("Capture sample rate is not supported.");
        setErrorMessage(err); setIsValid(false); emit errorOccurred(err); return false;
    }
    if (captureBitDepth_ != 0 && captureBitDepth_ != 16 && captureBitDepth_ != 24 && captureBitDepth_ != 32) {
        const QString err = QStringLiteral("Capture bit depth must be 16, 24, or 32 bits.");
        setErrorMessage(err); setIsValid(false); emit errorOccurred(err); return false;
    }

    setErrorMessage(QString());
    setIsValid(true);
    return true;
}
