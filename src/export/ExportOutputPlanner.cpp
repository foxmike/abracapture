#include "ExportOutputPlanner.hpp"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <system_error>

namespace abracapture::exporting {

namespace {

QString lowerExtension(const std::filesystem::path& path) {
    QString extension = QString::fromStdString(path.extension().string()).toLower();
    return extension.startsWith(QLatin1Char('.')) ? extension.mid(1) : extension;
}

bool isOccupied(const std::filesystem::path& path, const QSet<QString>& reservedOutputPaths) {
    return QFileInfo::exists(QString::fromStdString(path.string())) ||
           reservedOutputPaths.contains(ExportOutputPlanner::normalizedPathKey(path));
}

} // namespace

bool ExportOutputPlanner::plan(const ExportRequest& request,
                               const QSet<QString>& reservedOutputPaths,
                               ExportRequest* const plannedRequest,
                               QString* const error) {
    if (plannedRequest == nullptr || error == nullptr) {
        return false;
    }
    *error = {};
    if (request.destinationPath.empty()) {
        *error = QStringLiteral("Export destination path is empty");
        return false;
    }
    if (!request.profile.validate(error)) {
        return false;
    }

    std::error_code pathError;
    std::filesystem::path destination = std::filesystem::absolute(request.destinationPath, pathError);
    if (pathError) {
        *error = QStringLiteral("Unable to resolve export destination: %1")
                     .arg(QString::fromStdString(pathError.message()));
        return false;
    }
    if (destination.filename().empty()) {
        *error = QStringLiteral("Export destination must include a filename");
        return false;
    }

    const QString expectedExtension = exportFileExtension(request.profile.format);
    const QString extension = lowerExtension(destination);
    if (extension.isEmpty()) {
        destination.replace_extension(std::string(".") + expectedExtension.toStdString());
    } else if (extension != expectedExtension) {
        *error = QStringLiteral("Export destination extension must be .%1").arg(expectedExtension);
        return false;
    }

    const QFileInfo parentInfo(QString::fromStdString(destination.parent_path().string()));
    if (!parentInfo.exists() || !parentInfo.isDir()) {
        *error = QStringLiteral("Export destination directory does not exist");
        return false;
    }
    if (!request.sourcePath.empty() &&
        normalizedPathKey(request.sourcePath) == normalizedPathKey(destination)) {
        *error = QStringLiteral("Export destination must not be the source path");
        return false;
    }

    if (isOccupied(destination, reservedOutputPaths)) {
        if (request.profile.collisionPolicy == CollisionPolicy::FailIfExists) {
            *error = QStringLiteral("Export destination already exists");
            return false;
        }

        if (request.profile.collisionPolicy == CollisionPolicy::ReplaceExisting) {
            *plannedRequest = request;
            plannedRequest->destinationPath = destination;
            return true;
        }

        const std::filesystem::path parent = destination.parent_path();
        const std::string stem = destination.stem().string();
        const std::string extensionWithDot = destination.extension().string();
        bool found = false;
        for (int suffix = 1; suffix <= 9999; ++suffix) {
            const QString suffixText = QStringLiteral("_%1").arg(suffix, 3, 10, QLatin1Char('0'));
            const std::filesystem::path candidate = parent /
                (stem + suffixText.toStdString() + extensionWithDot);
            if (!isOccupied(candidate, reservedOutputPaths)) {
                destination = candidate;
                found = true;
                break;
            }
        }
        if (!found) {
            *error = QStringLiteral("Unable to allocate a non-conflicting export destination");
            return false;
        }
    }

    *plannedRequest = request;
    plannedRequest->destinationPath = destination;
    return true;
}

QString ExportOutputPlanner::normalizedPathKey(const std::filesystem::path& path) {
    return QDir::cleanPath(QFileInfo(QString::fromStdString(path.string())).absoluteFilePath());
}

} // namespace abracapture::exporting
