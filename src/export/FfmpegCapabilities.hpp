#pragma once

#include "AudioDomainTypes.hpp"

#include <QObject>
#include <QSet>
#include <QString>

namespace abracapture::exporting {

class FfmpegCapabilities : public QObject {
    Q_OBJECT

public:
    explicit FfmpegCapabilities(QObject* parent = nullptr);

    [[nodiscard]] bool probe();

    [[nodiscard]] QString ffmpegPath() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] bool isProbed() const noexcept;

    [[nodiscard]] bool isEncoderAvailable(const QString& encoderName) const;
    [[nodiscard]] bool isMuxerAvailable(const QString& muxerName) const;
    [[nodiscard]] bool isTargetAvailable(AudioContainer container, AudioCodec codec) const;

    [[nodiscard]] QString encoderForCodec(AudioCodec codec) const;
    [[nodiscard]] QString muxerForContainer(AudioContainer container) const;

signals:
    void probeCompleted();

private:
    QString ffmpegPath_;
    QString version_;
    QSet<QString> encoders_;
    QSet<QString> muxers_;
    bool probed_ = false;
};

} // namespace abracapture::exporting
