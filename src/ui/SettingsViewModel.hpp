#pragma once

#include "export/ExportTypes.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class SettingsViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString outputDirectory READ outputDirectory WRITE setOutputDirectory NOTIFY outputDirectoryChanged)
    Q_PROPERTY(QString namingPattern READ namingPattern WRITE setNamingPattern NOTIFY namingPatternChanged)
    Q_PROPERTY(QString fileNamingPattern READ namingPattern WRITE setNamingPattern NOTIFY namingPatternChanged)
    Q_PROPERTY(QString masterFormat READ masterFormat WRITE setMasterFormat NOTIFY masterFormatChanged)
    Q_PROPERTY(int captureSampleRate READ captureSampleRate WRITE setCaptureSampleRate NOTIFY captureSampleRateChanged)
    Q_PROPERTY(int captureBitDepth READ captureBitDepth WRITE setCaptureBitDepth NOTIFY captureBitDepthChanged)
    Q_PROPERTY(bool exportEnabled READ exportEnabled WRITE setExportEnabled NOTIFY exportEnabledChanged)
    Q_PROPERTY(QString exportFormat READ exportFormat WRITE setExportFormat NOTIFY exportFormatChanged)
    Q_PROPERTY(bool autoStartEnabled READ autoStartEnabled WRITE setAutoStartEnabled NOTIFY autoStartEnabledChanged)
    Q_PROPERTY(double autoStartThreshold READ autoStartThreshold WRITE setAutoStartThreshold NOTIFY autoStartThresholdChanged)
    Q_PROPERTY(bool autoStopEnabled READ autoStopEnabled WRITE setAutoStopEnabled NOTIFY autoStopEnabledChanged)
    Q_PROPERTY(int autoStopSilenceDuration READ autoStopSilenceDuration WRITE setAutoStopSilenceDuration NOTIFY autoStopSilenceDurationChanged)
    Q_PROPERTY(int preRollSeconds READ preRollSeconds WRITE setPreRollSeconds NOTIFY preRollSecondsChanged)
    Q_PROPERTY(int postRollSeconds READ postRollSeconds WRITE setPostRollSeconds NOTIFY postRollSecondsChanged)
    Q_PROPERTY(bool useMediaMetadataForAutomatedCaptureNames
                   READ useMediaMetadataForAutomatedCaptureNames
                   WRITE setUseMediaMetadataForAutomatedCaptureNames
                   NOTIFY useMediaMetadataForAutomatedCaptureNamesChanged)
    Q_PROPERTY(int automaticExportSampleRate READ automaticExportSampleRate
                   WRITE setAutomaticExportSampleRate NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(int automaticExportMp3Bitrate READ automaticExportMp3Bitrate
                   WRITE setAutomaticExportMp3Bitrate NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(int automaticExportFlacBitDepth READ automaticExportFlacBitDepth
                   WRITE setAutomaticExportFlacBitDepth NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(int automaticExportFlacCompressionLevel READ automaticExportFlacCompressionLevel
                   WRITE setAutomaticExportFlacCompressionLevel NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(QString automaticExportWavSampleFormat READ automaticExportWavSampleFormat
                   WRITE setAutomaticExportWavSampleFormat NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(QStringList automaticExportFormats READ automaticExportFormats CONSTANT)
    Q_PROPERTY(QStringList automaticExportMp3BitrateOptions READ automaticExportMp3BitrateOptions CONSTANT)
    Q_PROPERTY(QStringList automaticExportFlacBitDepthOptions READ automaticExportFlacBitDepthOptions CONSTANT)
    Q_PROPERTY(QStringList automaticExportFlacCompressionOptions READ automaticExportFlacCompressionOptions CONSTANT)
    Q_PROPERTY(QStringList automaticExportWavSampleFormatOptions READ automaticExportWavSampleFormatOptions CONSTANT)
    Q_PROPERTY(QStringList automaticExportSampleRateOptions READ automaticExportSampleRateOptions
                   NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(QStringList automaticExportPresetOptions READ automaticExportPresetOptions CONSTANT)
    Q_PROPERTY(QStringList automaticExportPresetLabels READ automaticExportPresetLabels CONSTANT)
    Q_PROPERTY(QStringList automaticExportSampleRateOptionLabels READ automaticExportSampleRateOptionLabels
                   NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(QStringList automaticExportMp3BitrateOptionLabels READ automaticExportMp3BitrateOptionLabels CONSTANT)
    Q_PROPERTY(QStringList automaticExportFlacBitDepthOptionLabels READ automaticExportFlacBitDepthOptionLabels CONSTANT)
    Q_PROPERTY(QStringList automaticExportFlacCompressionOptionLabels READ automaticExportFlacCompressionOptionLabels CONSTANT)
    Q_PROPERTY(QStringList automaticExportWavSampleFormatOptionLabels READ automaticExportWavSampleFormatOptionLabels CONSTANT)
    Q_PROPERTY(QString automaticExportDestinationPolicy READ automaticExportDestinationPolicy
                   WRITE setAutomaticExportDestinationPolicy NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(QString automaticExportCustomDirectory READ automaticExportCustomDirectory
                   WRITE setAutomaticExportCustomDirectory NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(QString automaticExportPreset READ automaticExportPreset
                   WRITE setAutomaticExportPreset NOTIFY automaticExportProfileChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY validityChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QStringList availableMasterFormats READ availableMasterFormats CONSTANT)
    Q_PROPERTY(QStringList availableExportFormats READ availableExportFormats CONSTANT)
    Q_PROPERTY(QStringList availableSampleRates READ availableSampleRates CONSTANT)
    Q_PROPERTY(QStringList availableBitDepths READ availableBitDepths CONSTANT)

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
    [[nodiscard]] int captureSampleRate() const noexcept;
    void setCaptureSampleRate(int sampleRate);
    [[nodiscard]] int captureBitDepth() const noexcept;
    void setCaptureBitDepth(int bitDepth);

    [[nodiscard]] bool exportEnabled() const noexcept;
    void setExportEnabled(bool exportEnabled);

    [[nodiscard]] QString exportFormat() const;
    void setExportFormat(const QString& exportFormat);
    [[nodiscard]] abracapture::exporting::AutomaticExportProfile automaticExportProfile() const;
    void setAutomaticExportProfile(
        const abracapture::exporting::AutomaticExportProfile& profile);

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

    [[nodiscard]] bool useMediaMetadataForAutomatedCaptureNames() const noexcept;
    void setUseMediaMetadataForAutomatedCaptureNames(bool enabled);

    [[nodiscard]] int automaticExportSampleRate() const noexcept;
    void setAutomaticExportSampleRate(int sampleRate);
    [[nodiscard]] int automaticExportMp3Bitrate() const noexcept;
    void setAutomaticExportMp3Bitrate(int bitrateKbps);
    [[nodiscard]] int automaticExportFlacBitDepth() const noexcept;
    void setAutomaticExportFlacBitDepth(int bitDepth);
    [[nodiscard]] int automaticExportFlacCompressionLevel() const noexcept;
    void setAutomaticExportFlacCompressionLevel(int compressionLevel);
    [[nodiscard]] QString automaticExportWavSampleFormat() const;
    void setAutomaticExportWavSampleFormat(const QString& sampleFormat);
    [[nodiscard]] QStringList automaticExportFormats() const;
    [[nodiscard]] QStringList automaticExportMp3BitrateOptions() const;
    [[nodiscard]] QStringList automaticExportFlacBitDepthOptions() const;
    [[nodiscard]] QStringList automaticExportFlacCompressionOptions() const;
    [[nodiscard]] QStringList automaticExportWavSampleFormatOptions() const;
    [[nodiscard]] QStringList automaticExportSampleRateOptions() const;
    [[nodiscard]] QStringList automaticExportPresetOptions() const;
    [[nodiscard]] QStringList automaticExportPresetLabels() const;
    [[nodiscard]] QStringList automaticExportSampleRateOptionLabels() const;
    [[nodiscard]] QStringList automaticExportMp3BitrateOptionLabels() const;
    [[nodiscard]] QStringList automaticExportFlacBitDepthOptionLabels() const;
    [[nodiscard]] QStringList automaticExportFlacCompressionOptionLabels() const;
    [[nodiscard]] QStringList automaticExportWavSampleFormatOptionLabels() const;
    [[nodiscard]] QString automaticExportDestinationPolicy() const;
    void setAutomaticExportDestinationPolicy(const QString& policy);
    [[nodiscard]] QString automaticExportCustomDirectory() const;
    void setAutomaticExportCustomDirectory(const QString& directory);
    [[nodiscard]] QString automaticExportPreset() const;
    void setAutomaticExportPreset(const QString& preset);
    Q_INVOKABLE QString automaticExportSummary() const;
    Q_INVOKABLE QString automaticExportPreviewPath(const QVariantMap& source) const;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] QString errorMessage() const;

    [[nodiscard]] QStringList availableMasterFormats() const;
    [[nodiscard]] QStringList availableExportFormats() const;
    [[nodiscard]] QStringList availableSampleRates() const;
    [[nodiscard]] QStringList availableBitDepths() const;

    Q_INVOKABLE void load();
    Q_INVOKABLE void save();
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE bool validate();

signals:
    void outputDirectoryChanged(const QString& outputDirectory);
    void namingPatternChanged(const QString& namingPattern);
    void fileNamingPatternChanged(const QString& fileNamingPattern);
    void masterFormatChanged(const QString& masterFormat);
    void captureSampleRateChanged(int sampleRate);
    void captureBitDepthChanged(int bitDepth);
    void exportEnabledChanged(bool exportEnabled);
    void exportFormatChanged(const QString& exportFormat);
    void autoStartEnabledChanged(bool autoStartEnabled);
    void autoStartThresholdChanged(double autoStartThreshold);
    void autoStopEnabledChanged(bool autoStopEnabled);
    void autoStopSilenceDurationChanged(int autoStopSilenceDuration);
    void preRollSecondsChanged(int preRollSeconds);
    void postRollSecondsChanged(int postRollSeconds);
    void useMediaMetadataForAutomatedCaptureNamesChanged(bool enabled);
    void automaticExportProfileChanged();
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
    int captureSampleRate_ = 0;
    int captureBitDepth_ = 0;
    abracapture::exporting::AutomaticExportProfile automaticExportProfile_;
    bool autoStartEnabled_ = false;
    double autoStartThreshold_ = -30.0;
    bool autoStopEnabled_ = false;
    int autoStopSilenceDuration_ = 3;
    int preRollSeconds_ = 0;
    int postRollSeconds_ = 0;
    bool useMediaMetadataForAutomatedCaptureNames_ = false;

    bool isValid_ = true;
    QString errorMessage_;
};
