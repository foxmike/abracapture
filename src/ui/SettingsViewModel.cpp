#include "SettingsViewModel.hpp"
#include "config/AppSettings.hpp"
#include "export/AudioFormatCatalog.hpp"
#include "export/ManualExportPlanner.hpp"
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
constexpr auto kAutomaticExportContainerKey = "automaticExport/container";
constexpr auto kAutomaticExportCodecKey = "automaticExport/codec";
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

using namespace abracapture::exporting;

QString sampleRatePolicyName(const SampleRatePolicy policy) {
    return policy == SampleRatePolicy::Explicit
               ? QStringLiteral("explicit")
               : QStringLiteral("preserve_source");
}

QString collisionPolicyName(const CollisionPolicy policy) {
    return policy == CollisionPolicy::FailIfExists
               ? QStringLiteral("fail")
               : QStringLiteral("generate_unique");
}

QString destinationPolicyName(const AutomaticExportDestinationPolicy policy) {
    switch (policy) {
    case AutomaticExportDestinationPolicy::SameFolder:
        return QStringLiteral("same");
    case AutomaticExportDestinationPolicy::ExportsSubfolder:
        return QStringLiteral("exports");
    case AutomaticExportDestinationPolicy::CustomFolder:
        return QStringLiteral("custom");
    }
    return QStringLiteral("same");
}

AutomaticExportDestinationPolicy destinationPolicyFromName(const QString& policy) {
    const QString normalized = policy.trimmed().toLower();
    if (normalized == QStringLiteral("exports")) {
        return AutomaticExportDestinationPolicy::ExportsSubfolder;
    }
    if (normalized == QStringLiteral("custom")) {
        return AutomaticExportDestinationPolicy::CustomFolder;
    }
    return AutomaticExportDestinationPolicy::SameFolder;
}

void saveAutomaticExportProfile(QSettings& settings,
                                 const AutomaticExportProfile& automaticProfile) {
    const auto& target = automaticProfile.profile.target;
    settings.setValue(QString::fromUtf8(kAutomaticExportEnabledKey), automaticProfile.enabled);
    settings.setValue(QString::fromUtf8(kAutomaticExportContainerKey),
                      audioContainerName(target.container));
    settings.setValue(QString::fromUtf8(kAutomaticExportCodecKey),
                      audioCodecName(target.codec));
    const auto legacyFormat = legacyFormatFor(target.container, target.codec);
    if (legacyFormat.has_value()) {
        settings.setValue(QString::fromUtf8(kAutomaticExportFormatKey),
                          exportFormatName(*legacyFormat));
    }
    settings.setValue(QString::fromUtf8(kAutomaticExportSampleRatePolicyKey),
                      sampleRatePolicyName(target.sampleRate.policy));
    settings.setValue(QString::fromUtf8(kAutomaticExportSampleRateKey),
                      static_cast<int>(target.sampleRate.sampleRate));
    settings.setValue(QString::fromUtf8(kAutomaticExportKeepMasterKey), automaticProfile.keepMaster);
    settings.setValue(QString::fromUtf8(kAutomaticExportCollisionPolicyKey),
                      collisionPolicyName(automaticProfile.profile.collisionPolicy));
    settings.setValue(QString::fromUtf8(kAutomaticExportDestinationPolicyKey),
                      destinationPolicyName(automaticProfile.destinationPolicy));
    settings.setValue(QString::fromUtf8(kAutomaticExportCustomDirectoryKey),
                      automaticProfile.customDestinationDirectory);

    if (const auto* const options = std::get_if<Mp3Options>(&target.codecOptions)) {
        settings.setValue(QString::fromUtf8(kAutomaticExportMp3BitrateKey),
                          static_cast<int>(options->bitrateKbps));
        settings.setValue(QString::fromUtf8(kAutomaticExportMp3BitrateModeKey),
                          options->bitrateMode == Mp3BitrateMode::Variable
                              ? QStringLiteral("vbr")
                              : QStringLiteral("cbr"));
    }
    if (const auto* const options = std::get_if<FlacOptions>(&target.codecOptions)) {
        settings.setValue(QString::fromUtf8(kAutomaticExportFlacBitDepthKey),
                          static_cast<int>(options->bitDepth));
        settings.setValue(QString::fromUtf8(kAutomaticExportFlacCompressionKey),
                          options->compressionLevel);
    }
    if (const auto* const options = std::get_if<PcmOptions>(&target.codecOptions)) {
        settings.setValue(QString::fromUtf8(kAutomaticExportWavSampleFormatKey),
                          static_cast<int>(options->sampleFormat));
    }
    if (const auto* const options = std::get_if<VorbisOptions>(&target.codecOptions)) {
        settings.setValue(QString::fromUtf8(kAutomaticExportOggQualityKey), options->quality);
    }
}

AutomaticExportProfile loadAutomaticExportProfile(QSettings& settings) {
    const bool hasNewFormat = settings.contains(QString::fromUtf8(kAutomaticExportContainerKey));
    const bool hasOldFormat = settings.contains(QString::fromUtf8(kAutomaticExportFormatKey));
    const bool legacyExportEnabled = settings.value(QStringLiteral("exportEnabled"), false).toBool();
    const QString legacyExportFormat =
        settings.value(QStringLiteral("exportFormat"), QStringLiteral("mp3")).toString();

    AudioContainer container = AudioContainer::Mp3;
    AudioCodec codec = AudioCodec::Mp3;

    if (hasNewFormat) {
        const auto parsedContainer = audioContainerFromString(
            settings.value(QString::fromUtf8(kAutomaticExportContainerKey)).toString());
        const auto parsedCodec = audioCodecFromString(
            settings.value(QString::fromUtf8(kAutomaticExportCodecKey)).toString());
        if (parsedContainer.has_value()) container = *parsedContainer;
        if (parsedCodec.has_value()) codec = *parsedCodec;
    } else {
        const QString formatStr = settings.value(
            QString::fromUtf8(kAutomaticExportFormatKey), legacyExportFormat).toString();
        const auto parsedFormat = exportFormatFromString(formatStr);
        const auto legacyFormat = parsedFormat.value_or(ExportFormat::Mp3);
        container = containerForLegacyFormat(legacyFormat);
        codec = codecForLegacyFormat(legacyFormat);
    }

    auto automaticProfile = AutomaticExportProfile{};
    automaticProfile.enabled = settings.value(
        QString::fromUtf8(kAutomaticExportEnabledKey), legacyExportEnabled).toBool();
    automaticProfile.keepMaster = settings.value(
        QString::fromUtf8(kAutomaticExportKeepMasterKey), true).toBool();
    automaticProfile.destinationPolicy = destinationPolicyFromName(settings.value(
        QString::fromUtf8(kAutomaticExportDestinationPolicyKey), QStringLiteral("same")).toString());
    automaticProfile.customDestinationDirectory = settings.value(
        QString::fromUtf8(kAutomaticExportCustomDirectoryKey)).toString();
    if (automaticProfile.destinationPolicy == AutomaticExportDestinationPolicy::CustomFolder &&
        automaticProfile.customDestinationDirectory.trimmed().isEmpty()) {
        automaticProfile.destinationPolicy = AutomaticExportDestinationPolicy::SameFolder;
    }

    automaticProfile.profile = defaultExportProfile(container, codec);
    const QString sampleRatePolicy = settings.value(
        QString::fromUtf8(kAutomaticExportSampleRatePolicyKey), QStringLiteral("preserve_source")).toString();
    if (sampleRatePolicy.trimmed().toLower() == QStringLiteral("explicit")) {
        automaticProfile.profile.target.sampleRate = {
            SampleRatePolicy::Explicit,
            static_cast<std::uint32_t>(settings.value(
                QString::fromUtf8(kAutomaticExportSampleRateKey), 0).toUInt())};
    }
    if (settings.value(QString::fromUtf8(kAutomaticExportCollisionPolicyKey),
                      QStringLiteral("generate_unique")).toString().trimmed().toLower() == QStringLiteral("fail")) {
        automaticProfile.profile.collisionPolicy = CollisionPolicy::FailIfExists;
    }

    switch (codec) {
    case AudioCodec::Mp3:
        automaticProfile.profile.target.codecOptions = Mp3Options{
            static_cast<std::uint32_t>(settings.value(
                QString::fromUtf8(kAutomaticExportMp3BitrateKey), 320).toUInt()),
            settings.value(QString::fromUtf8(kAutomaticExportMp3BitrateModeKey),
                           QStringLiteral("cbr")).toString().trimmed().toLower() == QStringLiteral("vbr")
                ? Mp3BitrateMode::Variable
                : Mp3BitrateMode::Constant};
        break;
    case AudioCodec::Flac:
        automaticProfile.profile.target.codecOptions = FlacOptions{
            static_cast<std::uint32_t>(settings.value(
                QString::fromUtf8(kAutomaticExportFlacBitDepthKey), 16).toUInt()),
            settings.value(QString::fromUtf8(kAutomaticExportFlacCompressionKey), 5).toInt()};
        break;
    case AudioCodec::Pcm:
        automaticProfile.profile.target.codecOptions = PcmOptions{
            static_cast<AudioSampleFormat>(settings.value(
                QString::fromUtf8(kAutomaticExportWavSampleFormatKey),
                static_cast<int>(AudioSampleFormat::SignedInt16)).toInt())};
        break;
    case AudioCodec::Vorbis:
        automaticProfile.profile.target.codecOptions = VorbisOptions{
            settings.value(QString::fromUtf8(kAutomaticExportOggQualityKey), 5).toInt()};
        break;
    }

    QString profileError;
    if (!automaticProfile.profile.validate(&profileError)) {
        automaticProfile.profile = defaultExportProfile(container, codec);
    }

    return automaticProfile;
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
    return abracapture::exporting::audioContainerName(
        automaticExportProfile_.profile.target.container);
}

void SettingsViewModel::setExportFormat(const QString& exportFormat) {
    const auto targetDesc = abracapture::exporting::AudioFormatCatalog::findTarget(exportFormat);
    if (!targetDesc.has_value()) {
        return;
    }
    if (automaticExportProfile_.profile.target.container == targetDesc->container &&
        automaticExportProfile_.profile.target.codec == targetDesc->codec) {
        return;
    }
    automaticExportProfile_.profile = abracapture::exporting::defaultExportProfile(
        targetDesc->container, targetDesc->codec);
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
    return automaticExportProfile_.profile.target.sampleRate.policy ==
                   abracapture::exporting::SampleRatePolicy::Explicit
               ? static_cast<int>(automaticExportProfile_.profile.target.sampleRate.sampleRate)
               : 0;
}

void SettingsViewModel::setAutomaticExportSampleRate(const int sampleRate) {
    auto profile = automaticExportProfile_;
    if (sampleRate == 0) {
        profile.profile.target.sampleRate = {abracapture::exporting::SampleRatePolicy::PreserveSource, 0};
    } else {
        if (!abracapture::exporting::supportsExplicitSampleRate(
                profile.profile.target.container, profile.profile.target.codec,
                static_cast<std::uint32_t>(sampleRate))) {
            return;
        }
        profile.profile.target.sampleRate = {abracapture::exporting::SampleRatePolicy::Explicit,
                                              static_cast<std::uint32_t>(sampleRate)};
    }
    setAutomaticExportProfile(profile);
}

int SettingsViewModel::automaticExportMp3Bitrate() const noexcept {
    if (const auto* const options = std::get_if<abracapture::exporting::Mp3Options>(
            &automaticExportProfile_.profile.target.codecOptions)) {
        return static_cast<int>(options->bitrateKbps);
    }
    return 320;
}

void SettingsViewModel::setAutomaticExportMp3Bitrate(const int bitrateKbps) {
    if (automaticExportProfile_.profile.target.codec != abracapture::exporting::AudioCodec::Mp3) {
        return;
    }
    auto profile = automaticExportProfile_;
    profile.profile.target.codecOptions = abracapture::exporting::Mp3Options{
        static_cast<std::uint32_t>(bitrateKbps), abracapture::exporting::Mp3BitrateMode::Constant};
    setAutomaticExportProfile(profile);
}

int SettingsViewModel::automaticExportFlacBitDepth() const noexcept {
    if (const auto* const options = std::get_if<abracapture::exporting::FlacOptions>(
            &automaticExportProfile_.profile.target.codecOptions)) {
        return static_cast<int>(options->bitDepth);
    }
    return 16;
}

void SettingsViewModel::setAutomaticExportFlacBitDepth(const int bitDepth) {
    if (automaticExportProfile_.profile.target.codec != abracapture::exporting::AudioCodec::Flac) {
        return;
    }
    auto profile = automaticExportProfile_;
    const auto* const current = std::get_if<abracapture::exporting::FlacOptions>(
        &profile.profile.target.codecOptions);
    profile.profile.target.codecOptions = abracapture::exporting::FlacOptions{
        static_cast<std::uint32_t>(bitDepth), current != nullptr ? current->compressionLevel : 5};
    setAutomaticExportProfile(profile);
}

int SettingsViewModel::automaticExportFlacCompressionLevel() const noexcept {
    if (const auto* const options = std::get_if<abracapture::exporting::FlacOptions>(
            &automaticExportProfile_.profile.target.codecOptions)) {
        return options->compressionLevel;
    }
    return 5;
}

void SettingsViewModel::setAutomaticExportFlacCompressionLevel(const int compressionLevel) {
    if (automaticExportProfile_.profile.target.codec != abracapture::exporting::AudioCodec::Flac) {
        return;
    }
    auto profile = automaticExportProfile_;
    const auto* const current = std::get_if<abracapture::exporting::FlacOptions>(
        &profile.profile.target.codecOptions);
    profile.profile.target.codecOptions = abracapture::exporting::FlacOptions{
        current != nullptr ? current->bitDepth : 16, compressionLevel};
    setAutomaticExportProfile(profile);
}

QString SettingsViewModel::automaticExportWavSampleFormat() const {
    if (const auto* const options = std::get_if<abracapture::exporting::PcmOptions>(
            &automaticExportProfile_.profile.target.codecOptions)) {
        return abracapture::exporting::audioSampleFormatName(options->sampleFormat);
    }
    return QStringLiteral("s16");
}

void SettingsViewModel::setAutomaticExportWavSampleFormat(const QString& sampleFormat) {
    if (automaticExportProfile_.profile.target.codec != abracapture::exporting::AudioCodec::Pcm) {
        return;
    }
    const auto parsed = abracapture::exporting::audioSampleFormatFromString(sampleFormat);
    if (!parsed.has_value()) {
        return;
    }
    auto profile = automaticExportProfile_;
    profile.profile.target.codecOptions = abracapture::exporting::PcmOptions{*parsed};
    setAutomaticExportProfile(profile);
}

QStringList SettingsViewModel::automaticExportFormats() const {
    QStringList formats;
    for (const auto& descriptor : abracapture::exporting::AudioFormatCatalog::targets()) {
        formats.append(abracapture::exporting::audioContainerName(descriptor.container));
    }
    return formats;
}

QStringList SettingsViewModel::automaticExportMp3BitrateOptions() const {
    return abracapture::exporting::AudioFormatCatalog::bitrateOptions(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
}

QStringList SettingsViewModel::automaticExportFlacBitDepthOptions() const {
    return abracapture::exporting::AudioFormatCatalog::bitDepthOptions(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
}

QStringList SettingsViewModel::automaticExportFlacCompressionOptions() const {
    return abracapture::exporting::AudioFormatCatalog::compressionLevelOptions(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
}

QStringList SettingsViewModel::automaticExportWavSampleFormatOptions() const {
    return abracapture::exporting::AudioFormatCatalog::sampleFormatOptions(
        abracapture::exporting::AudioContainer::Wav, abracapture::exporting::AudioCodec::Pcm);
}

QStringList SettingsViewModel::automaticExportSampleRateOptions() const {
    return abracapture::exporting::AudioFormatCatalog::sampleRateOptions(
        automaticExportProfile_.profile.target.container,
        automaticExportProfile_.profile.target.codec);
}

QStringList SettingsViewModel::automaticExportPresetOptions() const {
    return abracapture::exporting::AudioFormatCatalog::presetOptions();
}

QStringList SettingsViewModel::automaticExportPresetLabels() const {
    return abracapture::exporting::AudioFormatCatalog::presetOptionLabels();
}

QStringList SettingsViewModel::automaticExportSampleRateOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::sampleRateOptionLabels(
        automaticExportProfile_.profile.target.container,
        automaticExportProfile_.profile.target.codec);
}

QStringList SettingsViewModel::automaticExportMp3BitrateOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::bitrateOptionLabels(
        abracapture::exporting::AudioContainer::Mp3, abracapture::exporting::AudioCodec::Mp3);
}

QStringList SettingsViewModel::automaticExportFlacBitDepthOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::bitDepthOptionLabels(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
}

QStringList SettingsViewModel::automaticExportFlacCompressionOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::compressionLevelOptionLabels(
        abracapture::exporting::AudioContainer::Flac, abracapture::exporting::AudioCodec::Flac);
}

QStringList SettingsViewModel::automaticExportWavSampleFormatOptionLabels() const {
    return abracapture::exporting::AudioFormatCatalog::sampleFormatOptionLabels(
        abracapture::exporting::AudioContainer::Wav, abracapture::exporting::AudioCodec::Pcm);
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
    return abracapture::exporting::AudioFormatCatalog::findMatchingPreset(
        automaticExportProfile_.profile.target);
}

void SettingsViewModel::setAutomaticExportPreset(const QString& preset) {
    const auto target = abracapture::exporting::AudioFormatCatalog::targetFromPreset(preset);
    if (!target.has_value()) {
        return;
    }
    auto profile = automaticExportProfile_;
    profile.profile.target = *target;
    setAutomaticExportProfile(profile);
}

QString SettingsViewModel::automaticExportSummary() const {
    return abracapture::exporting::ManualExportPlanner::profileSummary(automaticExportProfile_.profile);
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
        "." + automaticExportProfile_.profile.target.fileExtension().toStdString());
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
    return automaticExportFormats();
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
        settings.contains(QString::fromUtf8(kAutomaticExportFormatKey)) ||
        settings.contains(QString::fromUtf8(kAutomaticExportContainerKey));
    auto automaticProfile = loadAutomaticExportProfile(settings);

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
