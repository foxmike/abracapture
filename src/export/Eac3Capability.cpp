#include "Eac3Capability.hpp"

#include <QProcess>
#include <QStandardPaths>

#include <algorithm>

namespace abracapture::exporting {
namespace {

bool looksLike71(const QString& name) {
    return name == QStringLiteral("7.1") ||
           name.startsWith(QStringLiteral("7.1(")) ||
           name.endsWith(QStringLiteral("(7.1)"));
}

QStringList tokenizeLayoutLine(const QString& line) {
    QStringList tokens;
    for (const QString& token : line.trimmed().split(QChar::Space, Qt::SkipEmptyParts)) {
        if (token != QStringLiteral("Supported") && token != QStringLiteral("channel") &&
            token != QStringLiteral("layouts:")) {
            tokens.append(token);
        }
    }
    return tokens;
}

void appendResolution(Eac3CapabilityReport& report, const QString& token,
                      std::uint32_t* const maxChannels) {
    if (token == QStringLiteral("mono") || token == QStringLiteral("stereo") ||
        token == QStringLiteral("quad") || token == QStringLiteral("3.0(back)") ||
        token == QStringLiteral("3.0") || token == QStringLiteral("4.0") ||
        token == QStringLiteral("5.0") || token == QStringLiteral("5.0(side)")) {
        *maxChannels = std::max<std::uint32_t>(*maxChannels, 2);
    } else if (token == QStringLiteral("2.1") || token == QStringLiteral("3.1") ||
               token == QStringLiteral("4.1")) {
        *maxChannels = std::max<std::uint32_t>(*maxChannels, 4);
    } else if (token == QStringLiteral("5.1") || token == QStringLiteral("5.1(side)") ||
               token == QStringLiteral("5.1(back)")) {
        *maxChannels = std::max<std::uint32_t>(*maxChannels, 6);
    } else if (looksLike71(token)) {
        *maxChannels = std::max<std::uint32_t>(*maxChannels, 8);
    }
    report.ffmpegSupportedLayouts.append(token);
}

} // namespace

bool Eac3CapabilityReport::supports71() const {
    return status == Status::SupportedBySystemFfmpeg;
}

QString Eac3CapabilityReport::statusName() const {
    switch (status) {
    case Status::SupportedBySystemFfmpeg: return QStringLiteral("SupportedBySystemFfmpeg");
    case Status::SystemFfmpegUnavailable: return QStringLiteral("SystemFfmpegUnavailable");
    case Status::SystemFfmpegLimitedTo51: return QStringLiteral("SystemFfmpegLimitedTo51");
    }
    return QStringLiteral("Unknown");
}

Eac3CapabilityReport Eac3CapabilityReport::unsupportedDefault() {
    return {};
}

QString Eac3CapabilityReport::supportedReason() const {
    if (status == Status::SupportedBySystemFfmpeg) {
        return QStringLiteral("E-AC-3 7.1 is supported by the installed FFmpeg (%1); "
                              "the encoder advertises the 7.1 layout.")
            .arg(ffmpegVersion.isEmpty() ? ffmpegPath : ffmpegVersion);
    }
    return {};
}

QString Eac3CapabilityReport::unsupportedReason() const {
    switch (status) {
    case Status::SystemFfmpegUnavailable:
        return QStringLiteral("E-AC-3 7.1 cannot be evaluated because the system FFmpeg "
                              "binary (%1) could not be probed.")
            .arg(ffmpegPath.isEmpty() ? QStringLiteral("(unknown)") : ffmpegPath);
    case Status::SystemFfmpegLimitedTo51:
        return QStringLiteral("The installed FFmpeg eac3 encoder advertises only up to "
                              "5.1 (%1 channels); 7.1 E-AC-3 is unavailable in this backend.")
            .arg(ffmpegMaxChannels);
    case Status::SupportedBySystemFfmpeg:
        return {};
    }
    return QStringLiteral("E-AC-3 7.1 capability is unavailable.");
}

QString Eac3Capability::detectFfmpegPath() {
    const QString found = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    return found.isEmpty() ? QStringLiteral("ffmpeg") : found;
}

QStringList Eac3Capability::parseSupportedLayouts(const QString& line) {
    return tokenizeLayoutLine(line);
}

bool Eac3Capability::layoutSupports71(const QStringList& layouts) {
    for (const QString& layout : layouts) {
        if (looksLike71(layout)) {
            return true;
        }
    }
    return false;
}

std::optional<Eac3CapabilityReport> Eac3Capability::probeSystemFfmpeg(
    const QString& ffmpegPath) {
    Eac3CapabilityReport report;
    report.ffmpegPath = ffmpegPath;

    QProcess encoderProcess;
    encoderProcess.start(ffmpegPath, {QStringLiteral("-hide_banner"), QStringLiteral("-h"),
                                      QStringLiteral("encoder=eac3")});
    if (!encoderProcess.waitForFinished(5000) ||
        encoderProcess.exitStatus() != QProcess::NormalExit || encoderProcess.exitCode() != 0) {
        report.status = Eac3CapabilityReport::Status::SystemFfmpegUnavailable;
        return report;
    }

    QProcess versionProcess;
    versionProcess.start(ffmpegPath, {QStringLiteral("-version")});
    if (versionProcess.waitForFinished(3000) && versionProcess.exitStatus() == QProcess::NormalExit &&
        versionProcess.exitCode() == 0) {
        const QStringList lines = QString::fromLocal8Bit(versionProcess.readAllStandardOutput())
                                       .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            const QStringList parts = lines.first().split(QChar::Space, Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                report.ffmpegVersion = parts.at(2);
            }
        }
    }

    const QString text = QString::fromLocal8Bit(
        encoderProcess.readAllStandardOutput() + encoderProcess.readAllStandardError());
    QStringList layouts;
    std::uint32_t maxChannels = 0;
    bool foundLayoutsLine = false;
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (line.contains(QStringLiteral("Supported channel layouts"))) {
            layouts = tokenizeLayoutLine(line);
            foundLayoutsLine = true;
            for (const QString& token : layouts) {
                appendResolution(report, token, &maxChannels);
            }
            report.ffmpegSupportedLayouts = layouts;
        } else if (!layouts.isEmpty()) {
            const QStringList wrapped = line.trimmed().split(QChar::Space, Qt::SkipEmptyParts);
            for (const QString& token : wrapped) {
                if (token == QStringLiteral("Encoder") || token == QStringLiteral("AVOptions:")) {
                    layouts.clear();
                    break;
                }
                appendResolution(report, token, &maxChannels);
            }
        }
    }
    if (!foundLayoutsLine) {
        report.status = Eac3CapabilityReport::Status::SystemFfmpegUnavailable;
        return report;
    }

    report.ffmpegProbed = true;
    report.ffmpegMaxChannels = maxChannels;
    report.status = layoutSupports71(report.ffmpegSupportedLayouts)
                        ? Eac3CapabilityReport::Status::SupportedBySystemFfmpeg
                        : Eac3CapabilityReport::Status::SystemFfmpegLimitedTo51;
    return report;
}

Eac3CapabilityReport Eac3Capability::probeNow() {
    const QString path = detectFfmpegPath();
    const auto report = probeSystemFfmpeg(path);
    return report.has_value() ? *report : Eac3CapabilityReport::unsupportedDefault();
}

} // namespace abracapture::exporting
