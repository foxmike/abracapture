#pragma once

#include "../recording/CaptureIntegrity.hpp"

#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>

#include <cstdint>
#include <filesystem>

namespace abracapture::exporting {

struct ExportSpec {
    QString format;
    std::uint32_t sampleRate = 0;
    std::uint32_t bitDepth = 0;
    QString codec;
    QString outputPath;
};

// Produces derived files from a completed master capture. Encoding is always
// performed by the external ffmpeg executable.
class ExportManager : public QObject {
    Q_OBJECT

public:
    explicit ExportManager(QObject* parent = nullptr);

    void startExports(const std::filesystem::path& masterFilePath,
                      const recording::CaptureIntegrity& integrity,
                      const QList<ExportSpec>& exportSpecs);

    [[nodiscard]] bool verifyMaster(const std::filesystem::path& masterFilePath,
                                    const recording::CaptureIntegrity& integrity) const;

signals:
    void exportStarted(QString format);
    void exportCompleted(QString path);
    void exportFailed(QString format, QString error);
    void allExportsFinished();

private:
    void startNextExport();

    QProcess process_;
    QString masterPath_;
    QList<ExportSpec> exportSpecs_;
    qsizetype currentExportIndex_ = 0;
    bool exportInFlight_ = false;
};

} // namespace abracapture::exporting
