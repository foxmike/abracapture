#include "CaptureIntegrity.hpp"

#include <QJsonValue>
#include <QString>

namespace abracapture::recording {

void CaptureIntegrity::setNegotiatedFormat(const SampleFormat format,
                                           const std::uint32_t rate) noexcept {
    negotiatedFormat = format;
    negotiatedRate = rate;
}

void CaptureIntegrity::setFileFormat(const SampleFormat format,
                                     const std::uint32_t rate) noexcept {
    fileFormat = format;
    fileRate = rate;
}

QJsonObject CaptureIntegrity::toJson() const {
    const CaptureKind kind = [this] {
        if (negotiatedFormat == fileFormat && negotiatedRate == fileRate) {
            return CaptureKind::Native;
        }
        if (negotiatedRate == fileRate) {
            return CaptureKind::Quantized;
        }
        if (negotiatedFormat == fileFormat) {
            return CaptureKind::Resampled;
        }
        return CaptureKind::Converted;
    }();

    const QString label = [](const CaptureKind value) {
        switch (value) {
        case CaptureKind::Native:
            return QStringLiteral("Native");
        case CaptureKind::Quantized:
            return QStringLiteral("Quantized");
        case CaptureKind::Resampled:
            return QStringLiteral("Resampled");
        case CaptureKind::Converted:
            return QStringLiteral("Converted");
        }

        return QString{};
    }(kind);

    const auto negotiatedName = sampleFormatName(negotiatedFormat);
    const auto fileName = sampleFormatName(fileFormat);

    return {
        {QStringLiteral("sourceStream"), QString::fromStdString(sourceStream)},
        {QStringLiteral("negotiatedFormat"),
         QString::fromStdString(std::string(negotiatedName))},
        {QStringLiteral("negotiatedRate"), static_cast<double>(negotiatedRate)},
        {QStringLiteral("fileFormat"), QString::fromStdString(std::string(fileName))},
        {QStringLiteral("fileRate"), static_cast<double>(fileRate)},
        {QStringLiteral("captureKind"), label},
        {QStringLiteral("xrunCount"), static_cast<double>(xrunCount)},
        {QStringLiteral("totalFrames"), static_cast<double>(totalFrames)},
        {QStringLiteral("durationSeconds"), durationSeconds},
        {QStringLiteral("filePath"), QString::fromStdString(filePath.string())},
    };
}

std::string CaptureIntegrity::summary() const {
    const QJsonObject report = toJson();
    return report.value(QStringLiteral("captureKind")).toString().toStdString() +
           ": " + filePath.string();
}

} // namespace abracapture::recording
