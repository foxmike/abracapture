#include "SettingsViewModel.hpp"
#include "config/AppSettings.hpp"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

namespace {
constexpr double kDefaultAutoStartThreshold = -30.0;
constexpr int kDefaultAutoStopSilenceDuration = 3;
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
    return exportEnabled_;
}

void SettingsViewModel::setExportEnabled(bool exportEnabled) {
    if (exportEnabled_ == exportEnabled) {
        return;
    }
    exportEnabled_ = exportEnabled;
    emit exportEnabledChanged(exportEnabled_);
    validate();
}

QString SettingsViewModel::exportFormat() const {
    return exportFormat_;
}

void SettingsViewModel::setExportFormat(const QString& exportFormat) {
    const QString normalized = exportFormat.trimmed().toLower();
    if (exportFormat_ == normalized) {
        return;
    }
    exportFormat_ = normalized;
    emit exportFormatChanged(exportFormat_);
    validate();
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
}

double SettingsViewModel::autoStartThreshold() const noexcept {
    return autoStartThreshold_;
}

void SettingsViewModel::setAutoStartThreshold(double autoStartThreshold) {
    if (qFuzzyCompare(autoStartThreshold_, autoStartThreshold)) {
        return;
    }
    autoStartThreshold_ = autoStartThreshold;
    emit autoStartThresholdChanged(autoStartThreshold_);
    validate();
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
}

int SettingsViewModel::autoStopSilenceDuration() const noexcept {
    return autoStopSilenceDuration_;
}

void SettingsViewModel::setAutoStopSilenceDuration(int autoStopSilenceDuration) {
    if (autoStopSilenceDuration < 0) {
        autoStopSilenceDuration = 0;
    }
    if (autoStopSilenceDuration_ == autoStopSilenceDuration) {
        return;
    }
    autoStopSilenceDuration_ = autoStopSilenceDuration;
    emit autoStopSilenceDurationChanged(autoStopSilenceDuration_);
    validate();
}

int SettingsViewModel::preRollSeconds() const noexcept {
    return preRollSeconds_;
}

void SettingsViewModel::setPreRollSeconds(int preRollSeconds) {
    if (preRollSeconds < 0) {
        preRollSeconds = 0;
    }
    if (preRollSeconds_ == preRollSeconds) {
        return;
    }
    preRollSeconds_ = preRollSeconds;
    emit preRollSecondsChanged(preRollSeconds_);
    validate();
}

int SettingsViewModel::postRollSeconds() const noexcept {
    return postRollSeconds_;
}

void SettingsViewModel::setPostRollSeconds(int postRollSeconds) {
    if (postRollSeconds < 0) {
        postRollSeconds = 0;
    }
    if (postRollSeconds_ == postRollSeconds) {
        return;
    }
    postRollSeconds_ = postRollSeconds;
    emit postRollSecondsChanged(postRollSeconds_);
    validate();
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
    const bool autoStart = appSettings.autoStartEnabled();
    const bool autoStop = appSettings.autoStopEnabled();
    const int preRoll = appSettings.preRollSeconds();
    const int postRoll = appSettings.postRollSeconds();

    const bool expEnabled = settings.value(QStringLiteral("exportEnabled"), false).toBool();
    QString expFormat = settings.value(QStringLiteral("exportFormat"), QStringLiteral("mp3")).toString().toLower();
    if (expFormat != QStringLiteral("mp3") && expFormat != QStringLiteral("flac") &&
        expFormat != QStringLiteral("ogg") && expFormat != QStringLiteral("wav")) {
        expFormat = QStringLiteral("mp3");
    }
    const double autoStartThresh = settings.value(QStringLiteral("autoStartThreshold"), kDefaultAutoStartThreshold).toDouble();
    int autoStopDuration = settings.value(QStringLiteral("autoStopSilenceDuration"), kDefaultAutoStopSilenceDuration).toInt();
    if (autoStopDuration < 0) {
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
    if (exportEnabled_ != expEnabled) {
        exportEnabled_ = expEnabled;
        emit exportEnabledChanged(exportEnabled_);
    }
    if (exportFormat_ != expFormat) {
        exportFormat_ = expFormat;
        emit exportFormatChanged(exportFormat_);
    }
    if (!qFuzzyCompare(autoStartThreshold_, autoStartThresh)) {
        autoStartThreshold_ = autoStartThresh;
        emit autoStartThresholdChanged(autoStartThreshold_);
    }
    if (autoStopSilenceDuration_ != autoStopDuration) {
        autoStopSilenceDuration_ = autoStopDuration;
        emit autoStopSilenceDurationChanged(autoStopSilenceDuration_);
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
    appSettings.save();

    QSettings settings(QStringLiteral("Abracapture"), QStringLiteral("Abracapture"));
    settings.setValue(QStringLiteral("exportEnabled"), exportEnabled_);
    settings.setValue(QStringLiteral("exportFormat"), exportFormat_);
    settings.setValue(QStringLiteral("autoStartThreshold"), autoStartThreshold_);
    settings.setValue(QStringLiteral("autoStopSilenceDuration"), autoStopSilenceDuration_);
    settings.sync();

    emit saved();
}

void SettingsViewModel::resetToDefaults() {
    setOutputDirectory(AppSettings::defaultOutputDirectory());
    setNamingPattern(AppSettings::defaultFileNamingPattern());
    setMasterFormat(AppSettings::defaultMasterFormat());
    setAutoStartEnabled(AppSettings::defaultAutoStartEnabled());
    setAutoStopEnabled(AppSettings::defaultAutoStopEnabled());
    setPreRollSeconds(AppSettings::defaultPreRollSeconds());
    setPostRollSeconds(AppSettings::defaultPostRollSeconds());
    setExportEnabled(false);
    setExportFormat(QStringLiteral("mp3"));
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

    if (autoStopSilenceDuration_ < 0 || autoStopSilenceDuration_ > 300) {
        const QString err = QStringLiteral("Auto-stop silence duration must be between 0 and 300 seconds.");
        setErrorMessage(err);
        setIsValid(false);
        emit errorOccurred(err);
        return false;
    }

    if (exportEnabled_) {
        const QString normExport = exportFormat_.trimmed().toLower();
        if (normExport != QStringLiteral("mp3") && normExport != QStringLiteral("flac") &&
            normExport != QStringLiteral("ogg") && normExport != QStringLiteral("wav")) {
            const QString err = QStringLiteral("Export format must be MP3, FLAC, OGG, or WAV.");
            setErrorMessage(err);
            setIsValid(false);
            emit errorOccurred(err);
            return false;
        }
    }

    setErrorMessage(QString());
    setIsValid(true);
    return true;
}
