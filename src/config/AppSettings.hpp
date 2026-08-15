#pragma once

#include <QObject>
#include <QString>

class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString outputDirectory READ outputDirectory WRITE setOutputDirectory NOTIFY outputDirectoryChanged)
    Q_PROPERTY(QString fileNamingPattern READ fileNamingPattern WRITE setFileNamingPattern NOTIFY fileNamingPatternChanged)
    Q_PROPERTY(QString masterFormat READ masterFormat WRITE setMasterFormat NOTIFY masterFormatChanged)
    Q_PROPERTY(bool autoStartEnabled READ autoStartEnabled WRITE setAutoStartEnabled NOTIFY autoStartEnabledChanged)
    Q_PROPERTY(bool autoStopEnabled READ autoStopEnabled WRITE setAutoStopEnabled NOTIFY autoStopEnabledChanged)
    Q_PROPERTY(int preRollSeconds READ preRollSeconds WRITE setPreRollSeconds NOTIFY preRollSecondsChanged)
    Q_PROPERTY(int postRollSeconds READ postRollSeconds WRITE setPostRollSeconds NOTIFY postRollSecondsChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString outputDirectory() const;
    void setOutputDirectory(const QString &outputDirectory);

    QString fileNamingPattern() const;
    void setFileNamingPattern(const QString &fileNamingPattern);

    QString masterFormat() const;
    void setMasterFormat(const QString &masterFormat);

    bool autoStartEnabled() const;
    void setAutoStartEnabled(bool autoStartEnabled);

    bool autoStopEnabled() const;
    void setAutoStopEnabled(bool autoStopEnabled);

    int preRollSeconds() const;
    void setPreRollSeconds(int preRollSeconds);

    int postRollSeconds() const;
    void setPostRollSeconds(int postRollSeconds);

    Q_INVOKABLE void load();
    Q_INVOKABLE void save();

    static QString defaultOutputDirectory();
    static QString defaultFileNamingPattern();
    static QString defaultMasterFormat();
    static bool defaultAutoStartEnabled();
    static bool defaultAutoStopEnabled();
    static int defaultPreRollSeconds();
    static int defaultPostRollSeconds();

signals:
    void outputDirectoryChanged(const QString &outputDirectory);
    void fileNamingPatternChanged(const QString &fileNamingPattern);
    void masterFormatChanged(const QString &masterFormat);
    void autoStartEnabledChanged(bool autoStartEnabled);
    void autoStopEnabledChanged(bool autoStopEnabled);
    void preRollSecondsChanged(int preRollSeconds);
    void postRollSecondsChanged(int postRollSeconds);

private:
    QString m_outputDirectory;
    QString m_fileNamingPattern;
    QString m_masterFormat;
    bool m_autoStartEnabled = false;
    bool m_autoStopEnabled = false;
    int m_preRollSeconds = 0;
    int m_postRollSeconds = 0;
};
