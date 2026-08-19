#pragma once

#include "AudioDomainTypes.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <vector>

namespace abracapture::exporting {

// Runtime capability report for E-AC-3 surround encoding.
//
// E-AC-3 7.1 is NOT supported by upstream open-source FFmpeg's libavcodec
// `eac3` encoder. The encoder's `acmod` (audio coding mode) topology table
// tops out at 5.1 (`3/2 + LFE`), reflecting Dolby's decision not to publish
// bit-allocation tables beyond that topology in the open-source tree.
//
// What this module guarantees:
//   * It never silently downgrades a 7.1 source to 5.1.
//   * It never claims 7.1 support from a layout string or a probe wrapper;
//     the active FFmpeg encoder must advertise it.
//   * A future real backend can be added without changing AudioDomainTypes.
struct Eac3CapabilityReport {
    enum class Status : std::uint8_t {
        // The system FFmpeg was probed; its eac3 encoder accepted the 7.1
        // layout explicitly listed in `Supported channel layouts:`.
        SupportedBySystemFfmpeg,
        // The system FFmpeg probe failed: FFmpeg binary missing, encoder
        // missing, probe error, or other parsing failure.
        SystemFfmpegUnavailable,
        // The system FFmpeg probe succeeded but the eac3 encoder's
        // supported channel-layout table tops out below 7.1.
        SystemFfmpegLimitedTo51,
    };

    Status status = Status::SystemFfmpegUnavailable;
    QString ffmpegPath;
    QString ffmpegVersion;
    QStringList ffmpegSupportedLayouts;
    std::uint32_t ffmpegMaxChannels = 0;
    bool ffmpegProbed = false;

    [[nodiscard]] bool supports71() const;
    [[nodiscard]] QString statusName() const;
    [[nodiscard]] QString supportedReason() const;
    // A precise, user-facing string explaining *exactly* why 7.1 is or is
    // not available. This is what the export pipeline shows when 7.1 fails.
    [[nodiscard]] QString unsupportedReason() const;

    static Eac3CapabilityReport unsupportedDefault();
};

class Eac3Capability {
public:
    // One-shot synchronous probe of the system FFmpeg eac3 encoder. UI code
    // invokes this from FfmpegCapabilities' worker and uses the cached report.
    [[nodiscard]] static Eac3CapabilityReport probeNow();

    // Probe helpers used by the model layer and by tests.
    [[nodiscard]] static QString detectFfmpegPath();
    [[nodiscard]] static std::optional<Eac3CapabilityReport> probeSystemFfmpeg(
        const QString& ffmpegPath);

    // Parses `Supported channel layouts:` line emitted by `ffmpeg -hide_banner
    // -h encoder=eac3`. The output is purely informational; the probe uses
    // this to compute `ffmpegMaxChannels` and an explicit `7.1` membership
    // test rather than faking it from the layout names alone.
    [[nodiscard]] static QStringList parseSupportedLayouts(const QString& line);

    // True iff a layout name recognized as 7.1 appears in the layout list.
    // Currently only the FFmpeg-style `7.1` literal — the FFmpeg encoder
    // names are identical for AC-3 and E-AC-3, so we keep the bar there.
    [[nodiscard]] static bool layoutSupports71(const QStringList& layouts);
};

} // namespace abracapture::exporting
