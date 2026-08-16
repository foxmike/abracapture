#include "ExportManager.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTimer>

#include <system_error>

namespace abracapture::exporting {

namespace {

QString normalizedFormat(const QString& format) {
    return format.trimmed().toLower();
}

void appendResamplingArguments(QStringList& arguments, const ExportSpec& spec) {
    if (spec.sampleRate != 0) {
        arguments << QStringLiteral("-ar") << QString::number(spec.sampleRate);
    }
}

void appendFlacBitDepthArguments(QStringList& arguments, const std::uint32_t bitDepth) {
    if (bitDepth == 16) {
        arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s16");
    } else if (bitDepth == 24 || bitDepth == 32) {
        arguments << QStringLiteral("-sample_fmt") << QStringLiteral("s32")
                  << QStringLiteral("-bits_per_raw_sample") << QString::number(bitDepth);
    }
}

QString wavCodecForBitDepth(const std::uint32_t bitDepth) {
    switch (bitDepth) {
    case 8:
        return QStringLiteral("pcm_u8");
    case 16:
        return QStringLiteral("pcm_s16le");
    case 24:
        return QStringLiteral("pcm_s24le");
    case 32:
        return QStringLiteral("pcm_s32le");
    default:
        return QStringLiteral("pcm_s16le");
    }
}

bool buildFfmpegArguments(const QString& masterPath,
                          const ExportSpec& spec,
                          QStringList* arguments,
                          QString* error) {
    const QString format = normalizedFormat(spec.format);
    if (format != QStringLiteral("flac") && format != QStringLiteral("mp3") &&
        format != QStringLiteral("ogg") && format != QStringLiteral("vorbis") &&
        format != QStringLiteral("wav") && format != QStringLiteral("wave")) {
        *error = QStringLiteral("Unsupported export format: %1").arg(spec.format);
        return false;
    }

    if (spec.outputPath.trimmed().isEmpty()) {
        *error = QStringLiteral("Export output path is empty");
        return false;
    }

    *arguments = {QStringLiteral("-nostdin"), QStringLiteral("-y"),
                  QStringLiteral("-i"), masterPath};
    appendResamplingArguments(*arguments, spec);

    if (format == QStringLiteral("flac")) {
        *arguments << QStringLiteral("-c:a")
                   << (spec.codec.isEmpty() ? QStringLiteral("flac") : spec.codec);
        appendFlacBitDepthArguments(*arguments, spec.bitDepth);
    } else if (format == QStringLiteral("mp3")) {
        *arguments << QStringLiteral("-c:a")
                   << (spec.codec.isEmpty() ? QStringLiteral("libmp3lame") : spec.codec)
                   << QStringLiteral("-q:a") << QStringLiteral("2");
    } else if (format == QStringLiteral("ogg") || format == QStringLiteral("vorbis")) {
        *arguments << QStringLiteral("-c:a")
                   << (spec.codec.isEmpty() ? QStringLiteral("libvorbis") : spec.codec)
                   << QStringLiteral("-q:a") << QStringLiteral("5")
                   << QStringLiteral("-f") << QStringLiteral("ogg");
    } else {
        *arguments << QStringLiteral("-c:a")
                   << (spec.codec.isEmpty() ? wavCodecForBitDepth(spec.bitDepth) : spec.codec);
    }

    *arguments << spec.outputPath;
    return true;
}

bool isMasterOutputPath(const QString& masterPath, const QString& outputPath) {
    std::error_code error;
    if (std::filesystem::equivalent(masterPath.toStdString(), outputPath.toStdString(), error) &&
        !error) {
        return true;
    }

    const QString masterAbsolute = QDir::cleanPath(QFileInfo(masterPath).absoluteFilePath());
    const QString outputAbsolute = QDir::cleanPath(QFileInfo(outputPath).absoluteFilePath());
    return masterAbsolute == outputAbsolute;
}

QString processError(QProcess& process, const int exitCode) {
    const QString standardError = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    if (!standardError.isEmpty()) {
        return standardError;
    }
    return QStringLiteral("ffmpeg exited with code %1").arg(exitCode);
}

} // namespace

ExportManager::ExportManager(QObject* const parent)
    : QObject(parent),
      process_(this) {
    connect(&process_, &QProcess::finished, this,
            [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
                if (!exportInFlight_) {
                    return;
                }

                const ExportSpec spec = exportSpecs_.at(currentExportIndex_);
                exportInFlight_ = false;
                const QFileInfo outputInfo(spec.outputPath);
                if (exitStatus == QProcess::NormalExit && exitCode == 0 &&
                    outputInfo.exists() && outputInfo.isFile() && outputInfo.size() > 0) {
                    emit exportCompleted(outputInfo.absoluteFilePath());
                } else if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    emit exportFailed(spec.format,
                                       QStringLiteral("ffmpeg completed but the export output is missing or empty"));
                } else {
                    emit exportFailed(spec.format, processError(process_, exitCode));
                }

                ++currentExportIndex_;
                QTimer::singleShot(0, this, &ExportManager::startNextExport);
            });

    connect(&process_, &QProcess::errorOccurred, this,
            [this](const QProcess::ProcessError error) {
                if (!exportInFlight_ || error != QProcess::FailedToStart) {
                    return;
                }

                const ExportSpec spec = exportSpecs_.at(currentExportIndex_);
                exportInFlight_ = false;
                emit exportFailed(spec.format,
                                   QStringLiteral("Unable to start ffmpeg: %1").arg(process_.errorString()));
                ++currentExportIndex_;
                QTimer::singleShot(0, this, &ExportManager::startNextExport);
            });
}

void ExportManager::startExports(const std::filesystem::path& masterFilePath,
                                 const recording::CaptureIntegrity& integrity,
                                 const QList<ExportSpec>& exportSpecs) {
    if (exportInFlight_) {
        emit exportFailed(QStringLiteral("exports"),
                           QStringLiteral("An export sequence is already running"));
        return;
    }

    masterPath_ = QString::fromStdString(masterFilePath.string());
    exportSpecs_ = exportSpecs;
    currentExportIndex_ = 0;

    if (!verifyMaster(masterFilePath, integrity)) {
        const QString error = QStringLiteral("Master capture is missing, not a regular file, empty, or does not match CaptureIntegrity");
        if (exportSpecs_.isEmpty()) {
            emit exportFailed(QStringLiteral("master"), error);
        } else {
            for (const ExportSpec& spec : exportSpecs_) {
                emit exportFailed(spec.format, error);
            }
        }
        emit allExportsFinished();
        return;
    }

    startNextExport();
}

bool ExportManager::verifyMaster(const std::filesystem::path& masterFilePath,
                                 const recording::CaptureIntegrity& integrity) const {
    const QFileInfo masterInfo(QString::fromStdString(masterFilePath.string()));
    if (!masterInfo.exists() || !masterInfo.isFile() || masterInfo.size() == 0) {
        return false;
    }

    if (integrity.filePath.empty()) {
        return true;
    }

    const QFileInfo integrityInfo(QString::fromStdString(integrity.filePath.string()));
    return QDir::cleanPath(masterInfo.absoluteFilePath()) ==
           QDir::cleanPath(integrityInfo.absoluteFilePath());
}

void ExportManager::startNextExport() {
    if (currentExportIndex_ >= exportSpecs_.size()) {
        emit allExportsFinished();
        return;
    }

    const ExportSpec& spec = exportSpecs_.at(currentExportIndex_);
    QStringList arguments;
    QString error;
    if (!buildFfmpegArguments(masterPath_, spec, &arguments, &error)) {
        emit exportFailed(spec.format, error);
        ++currentExportIndex_;
        QTimer::singleShot(0, this, &ExportManager::startNextExport);
        return;
    }

    if (isMasterOutputPath(masterPath_, spec.outputPath)) {
        emit exportFailed(spec.format,
                           QStringLiteral("Export output path must not be the master capture path"));
        ++currentExportIndex_;
        QTimer::singleShot(0, this, &ExportManager::startNextExport);
        return;
    }

    exportInFlight_ = true;
    emit exportStarted(spec.format);
    process_.start(QStringLiteral("ffmpeg"), arguments);
}

} // namespace abracapture::exporting
