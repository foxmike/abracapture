#pragma once

#include "AudioDomainTypes.hpp"
#include "Eac3Capability.hpp"

#include <QObject>
#include <QSet>
#include <QString>

namespace abracapture::exporting {

class FfmpegCapabilities : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool probed READ isProbed NOTIFY probeCompleted)
    Q_PROPERTY(bool probing READ isProbing NOTIFY probingChanged)
    Q_PROPERTY(bool eac3Supports71 READ eac3Supports71 NOTIFY eac3CapabilityChanged)
    Q_PROPERTY(QString eac3UnsupportedReason READ eac3UnsupportedReason NOTIFY eac3CapabilityChanged)

public:
    explicit FfmpegCapabilities(QObject* parent = nullptr);
    ~FfmpegCapabilities() override = default;

    [[nodiscard]] bool probe();
    void probeAsync();

    // Re-runs the E-AC-3 capability probe alone (no encoder/muxer enumeration).
    // Use when the export pipeline expects the Eac3 capability to drift without
    // bothering the rest of the catalog.
    [[nodiscard]] Eac3CapabilityReport refreshEac3Capability();

    [[nodiscard]] QString ffmpegPath() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] bool isProbed() const noexcept;
    [[nodiscard]] bool isProbing() const noexcept;

    [[nodiscard]] bool isEncoderAvailable(const QString& encoderName) const;
    [[nodiscard]] bool isMuxerAvailable(const QString& muxerName) const;
    [[nodiscard]] bool isTargetAvailable(AudioContainer container, AudioCodec codec) const;

    [[nodiscard]] QString encoderForCodec(AudioCodec codec) const;
    [[nodiscard]] QString muxerForContainer(AudioContainer container) const;

    [[nodiscard]] const Eac3CapabilityReport& eac3Capability() const noexcept;
    [[nodiscard]] bool eac3Supports71() const noexcept;
    [[nodiscard]] QString eac3UnsupportedReason() const;

signals:
    void probeCompleted();
    void probingChanged();
    void eac3CapabilityChanged();

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
    Eac3CapabilityReport eac3Capability_;
};

} // namespace abracapture::exporting
