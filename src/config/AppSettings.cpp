#include "AppSettings.hpp"

#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr const char *kOrgName = "Abracapture";
constexpr const char *kAppName = "Abracapture";

constexpr const char *kKeyOutputDirectory = "outputDirectory";
constexpr const char *kKeyFileNamingPattern = "fileNamingPattern";
constexpr const char *kKeyMasterFormat = "masterFormat";
constexpr const char *kKeyAutoStartEnabled = "autoStartEnabled";
constexpr const char *kKeyAutoStopEnabled = "autoStopEnabled";
constexpr const char *kKeyPreRollSeconds = "preRollSeconds";
constexpr const char *kKeyPostRollSeconds = "postRollSeconds";
} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent) {
    load();
}

QString AppSettings::defaultOutputDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + QStringLiteral("/Abracapture");
}

QString AppSettings::defaultFileNamingPattern() {
    return QStringLiteral("{source}_{date}_{time}");
}

QString AppSettings::defaultMasterFormat() {
    return QStringLiteral("wav");
}

bool AppSettings::defaultAutoStartEnabled() {
    return false;
}

bool AppSettings::defaultAutoStopEnabled() {
    return false;
}

int AppSettings::defaultPreRollSeconds() {
    return 0;
}

int AppSettings::defaultPostRollSeconds() {
    return 0;
}

QString AppSettings::outputDirectory() const {
    return m_outputDirectory;
}

void AppSettings::setOutputDirectory(const QString &outputDirectory) {
    if (m_outputDirectory == outputDirectory) {
        return;
    }
    m_outputDirectory = outputDirectory;
    save();
    emit outputDirectoryChanged(m_outputDirectory);
}

QString AppSettings::fileNamingPattern() const {
    return m_fileNamingPattern;
}

void AppSettings::setFileNamingPattern(const QString &fileNamingPattern) {
    if (m_fileNamingPattern == fileNamingPattern) {
        return;
    }
    m_fileNamingPattern = fileNamingPattern;
    save();
    emit fileNamingPatternChanged(m_fileNamingPattern);
}

QString AppSettings::masterFormat() const {
    return m_masterFormat;
}

void AppSettings::setMasterFormat(const QString &masterFormat) {
    QString normalized = masterFormat.toLower();
    if (normalized != QStringLiteral("wav") && normalized != QStringLiteral("flac")) {
        return;
    }
    if (m_masterFormat == normalized) {
        return;
    }
    m_masterFormat = normalized;
    save();
    emit masterFormatChanged(m_masterFormat);
}

bool AppSettings::autoStartEnabled() const {
    return m_autoStartEnabled;
}

void AppSettings::setAutoStartEnabled(bool autoStartEnabled) {
    if (m_autoStartEnabled == autoStartEnabled) {
        return;
    }
    m_autoStartEnabled = autoStartEnabled;
    save();
    emit autoStartEnabledChanged(m_autoStartEnabled);
}

bool AppSettings::autoStopEnabled() const {
    return m_autoStopEnabled;
}

void AppSettings::setAutoStopEnabled(bool autoStopEnabled) {
    if (m_autoStopEnabled == autoStopEnabled) {
        return;
    }
    m_autoStopEnabled = autoStopEnabled;
    save();
    emit autoStopEnabledChanged(m_autoStopEnabled);
}

int AppSettings::preRollSeconds() const {
    return m_preRollSeconds;
}

void AppSettings::setPreRollSeconds(int preRollSeconds) {
    if (preRollSeconds < 0) {
        preRollSeconds = 0;
    }
    if (m_preRollSeconds == preRollSeconds) {
        return;
    }
    m_preRollSeconds = preRollSeconds;
    save();
    emit preRollSecondsChanged(m_preRollSeconds);
}

int AppSettings::postRollSeconds() const {
    return m_postRollSeconds;
}

void AppSettings::setPostRollSeconds(int postRollSeconds) {
    if (postRollSeconds < 0) {
        postRollSeconds = 0;
    }
    if (m_postRollSeconds == postRollSeconds) {
        return;
    }
    m_postRollSeconds = postRollSeconds;
    save();
    emit postRollSecondsChanged(m_postRollSeconds);
}

void AppSettings::load() {
    QSettings settings(QString::fromUtf8(kOrgName), QString::fromUtf8(kAppName));

    const QString outDir = settings.value(QString::fromUtf8(kKeyOutputDirectory), defaultOutputDirectory()).toString();
    const QString pattern = settings.value(QString::fromUtf8(kKeyFileNamingPattern), defaultFileNamingPattern()).toString();
    QString format = settings.value(QString::fromUtf8(kKeyMasterFormat), defaultMasterFormat()).toString().toLower();
    if (format != QStringLiteral("wav") && format != QStringLiteral("flac")) {
        format = defaultMasterFormat();
    }
    const bool autoStart = settings.value(QString::fromUtf8(kKeyAutoStartEnabled), defaultAutoStartEnabled()).toBool();
    const bool autoStop = settings.value(QString::fromUtf8(kKeyAutoStopEnabled), defaultAutoStopEnabled()).toBool();
    int preRoll = settings.value(QString::fromUtf8(kKeyPreRollSeconds), defaultPreRollSeconds()).toInt();
    if (preRoll < 0) {
        preRoll = 0;
    }
    int postRoll = settings.value(QString::fromUtf8(kKeyPostRollSeconds), defaultPostRollSeconds()).toInt();
    if (postRoll < 0) {
        postRoll = 0;
    }

    if (m_outputDirectory != outDir) {
        m_outputDirectory = outDir;
        emit outputDirectoryChanged(m_outputDirectory);
    }
    if (m_fileNamingPattern != pattern) {
        m_fileNamingPattern = pattern;
        emit fileNamingPatternChanged(m_fileNamingPattern);
    }
    if (m_masterFormat != format) {
        m_masterFormat = format;
        emit masterFormatChanged(m_masterFormat);
    }
    if (m_autoStartEnabled != autoStart) {
        m_autoStartEnabled = autoStart;
        emit autoStartEnabledChanged(m_autoStartEnabled);
    }
    if (m_autoStopEnabled != autoStop) {
        m_autoStopEnabled = autoStop;
        emit autoStopEnabledChanged(m_autoStopEnabled);
    }
    if (m_preRollSeconds != preRoll) {
        m_preRollSeconds = preRoll;
        emit preRollSecondsChanged(m_preRollSeconds);
    }
    if (m_postRollSeconds != postRoll) {
        m_postRollSeconds = postRoll;
        emit postRollSecondsChanged(m_postRollSeconds);
    }
}

void AppSettings::save() {
    QSettings settings(QString::fromUtf8(kOrgName), QString::fromUtf8(kAppName));
    settings.setValue(QString::fromUtf8(kKeyOutputDirectory), m_outputDirectory);
    settings.setValue(QString::fromUtf8(kKeyFileNamingPattern), m_fileNamingPattern);
    settings.setValue(QString::fromUtf8(kKeyMasterFormat), m_masterFormat);
    settings.setValue(QString::fromUtf8(kKeyAutoStartEnabled), m_autoStartEnabled);
    settings.setValue(QString::fromUtf8(kKeyAutoStopEnabled), m_autoStopEnabled);
    settings.setValue(QString::fromUtf8(kKeyPreRollSeconds), m_preRollSeconds);
    settings.setValue(QString::fromUtf8(kKeyPostRollSeconds), m_postRollSeconds);
    settings.sync();
}
