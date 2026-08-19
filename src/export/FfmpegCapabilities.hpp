#pragma once

#include "AudioDomainTypes.hpp"

#include <QObject>
#include <QSet>
#include <QString>

namespace abracapture::exporting {

class FfmpegCapabilities : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool probed READ isProbed NOTIFY probeCompleted)
    Q_PROPERTY(bool probing READ isProbing NOTIFY probingChanged)

public:
    explicit FfmpegCapabilities(QObject* parent = nullptr);
    ~FfmpegCapabilities() override = default;

    [[nodiscard]] bool probe();
    void probeAsync();

    [[nodiscard]] QString ffmpegPath() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] bool isProbed() const noexcept;
    [[nodiscard]] bool isProbing() const noexcept;

    [[nodiscard]] bool isEncoderAvailable(const QString& encoderName) const;
    [[nodiscard]] bool isMuxerAvailable(const QString& muxerName) const;
    [[nodiscard]] bool isTargetAvailable(AudioContainer container, AudioCodec codec) const;

    [[nodiscard]] QString encoderForCodec(AudioCodec codec) const;
    [[nodiscard]] QString muxerForContainer(AudioContainer container) const;

signals:
    void probeCompleted();
    void probingChanged();

private:
    struct ProbeResult {
        QString ffmpegPath;
        QString version;
        QSet<QString> encoders;
        QSet<QString> muxers;
        bool success = false;
    };

    [[nodiscard]] static ProbeResult executeProbe();

    QString ffmpegPath_;
    QString version_;
    QSet<QString> encoders_;
    QSet<QString> muxers_;
    bool probed_ = false;
    bool probing_ = false;
};

} // namespace abracapture::exporting
