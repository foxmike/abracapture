#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class SettingsViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString outputDirectory READ outputDirectory WRITE setOutputDirectory NOTIFY outputDirectoryChanged)
    Q_PROPERTY(QString namingPattern READ namingPattern WRITE setNamingPattern NOTIFY namingPatternChanged)
    Q_PROPERTY(QString fileNamingPattern READ namingPattern WRITE setNamingPattern NOTIFY namingPatternChanged)
    Q_PROPERTY(QString masterFormat READ masterFormat WRITE setMasterFormat NOTIFY masterFormatChanged)
    Q_PROPERTY(bool exportEnabled READ exportEnabled WRITE setExportEnabled NOTIFY exportEnabledChanged)
    Q_PROPERTY(QString exportFormat READ exportFormat WRITE setExportFormat NOTIFY exportFormatChanged)
    Q_PROPERTY(bool autoStartEnabled READ autoStartEnabled WRITE setAutoStartEnabled NOTIFY autoStartEnabledChanged)
    Q_PROPERTY(double autoStartThreshold READ autoStartThreshold WRITE setAutoStartThreshold NOTIFY autoStartThresholdChanged)
    Q_PROPERTY(bool autoStopEnabled READ autoStopEnabled WRITE setAutoStopEnabled NOTIFY autoStopEnabledChanged)
    Q_PROPERTY(int autoStopSilenceDuration READ autoStopSilenceDuration WRITE setAutoStopSilenceDuration NOTIFY autoStopSilenceDurationChanged)
    Q_PROPERTY(int preRollSeconds READ preRollSeconds WRITE setPreRollSeconds NOTIFY preRollSecondsChanged)
    Q_PROPERTY(int postRollSeconds READ postRollSeconds WRITE setPostRollSeconds NOTIFY postRollSecondsChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY validityChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QStringList availableMasterFormats READ availableMasterFormats CONSTANT)
    Q_PROPERTY(QStringList availableExportFormats READ availableExportFormats CONSTANT)

public:
    enum class MasterFormatOption {
        Wav,
        Flac
    };
    Q_ENUM(MasterFormatOption)

    explicit SettingsViewModel(QObject* parent = nullptr);
    ~SettingsViewModel() override = default;

    [[nodiscard]] QString outputDirectory() const;
    void setOutputDirectory(const QString& outputDirectory);

    [[nodiscard]] QString namingPattern() const;
    void setNamingPattern(const QString& namingPattern);

    [[nodiscard]] QString masterFormat() const;
    void setMasterFormat(const QString& masterFormat);

    [[nodiscard]] bool exportEnabled() const noexcept;
    void setExportEnabled(bool exportEnabled);

    [[nodiscard]] QString exportFormat() const;
    void setExportFormat(const QString& exportFormat);

    [[nodiscard]] bool autoStartEnabled() const noexcept;
    void setAutoStartEnabled(bool autoStartEnabled);

    [[nodiscard]] double autoStartThreshold() const noexcept;
    void setAutoStartThreshold(double autoStartThreshold);

    [[nodiscard]] bool autoStopEnabled() const noexcept;
    void setAutoStopEnabled(bool autoStopEnabled);

    [[nodiscard]] int autoStopSilenceDuration() const noexcept;
    void setAutoStopSilenceDuration(int autoStopSilenceDuration);

    [[nodiscard]] int preRollSeconds() const noexcept;
    void setPreRollSeconds(int preRollSeconds);

    [[nodiscard]] int postRollSeconds() const noexcept;
    void setPostRollSeconds(int postRollSeconds);

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] QString errorMessage() const;

    [[nodiscard]] QStringList availableMasterFormats() const;
    [[nodiscard]] QStringList availableExportFormats() const;

    Q_INVOKABLE void load();
    Q_INVOKABLE void save();
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE bool validate();

signals:
    void outputDirectoryChanged(const QString& outputDirectory);
    void namingPatternChanged(const QString& namingPattern);
    void fileNamingPatternChanged(const QString& fileNamingPattern);
    void masterFormatChanged(const QString& masterFormat);
    void exportEnabledChanged(bool exportEnabled);
    void exportFormatChanged(const QString& exportFormat);
    void autoStartEnabledChanged(bool autoStartEnabled);
    void autoStartThresholdChanged(double autoStartThreshold);
    void autoStopEnabledChanged(bool autoStopEnabled);
    void autoStopSilenceDurationChanged(int autoStopSilenceDuration);
    void preRollSecondsChanged(int preRollSeconds);
    void postRollSecondsChanged(int postRollSeconds);
    void validityChanged(bool isValid);
    void errorMessageChanged(const QString& errorMessage);
    void errorOccurred(const QString& errorMessage);
    void saved();

private:
    void setErrorMessage(const QString& message);
    void setIsValid(bool valid);

    QString outputDirectory_;
    QString namingPattern_;
    QString masterFormat_ = QStringLiteral("wav");
    bool exportEnabled_ = false;
    QString exportFormat_ = QStringLiteral("mp3");
    bool autoStartEnabled_ = false;
    double autoStartThreshold_ = -30.0;
    bool autoStopEnabled_ = false;
    int autoStopSilenceDuration_ = 3;
    int preRollSeconds_ = 0;
    int postRollSeconds_ = 0;

    bool isValid_ = true;
    QString errorMessage_;
};
