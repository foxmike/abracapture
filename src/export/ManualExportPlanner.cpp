#include "ManualExportPlanner.hpp"

#include "../recording/OutputPathPolicy.hpp"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <system_error>

namespace abracapture::exporting {

namespace {

QString normalizedPath(const std::filesystem::path& path) {
    return QDir::cleanPath(QFileInfo(QString::fromStdString(path.string())).absoluteFilePath());
}

bool isInsideDirectory(const QString& path, const QString& directory) {
    if (path == directory) {
        return true;
    }
    return path.startsWith(directory + QDir::separator());
}

bool isPlaceholder(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    return normalized.isEmpty() || normalized == QStringLiteral("unknown") ||
           normalized == QStringLiteral("unknown album") || normalized == QStringLiteral("untitled") ||
           normalized == QStringLiteral("null") || normalized == QStringLiteral("n/a");
}

QString sampleRateSummary(const ExportProfile& profile) {
    if (profile.sampleRate.policy == SampleRatePolicy::PreserveSource) {
        return QStringLiteral("Preserve source rate");
    }
    const double kilohertz = static_cast<double>(profile.sampleRate.sampleRate) / 1000.0;
    return QStringLiteral("%1 kHz").arg(kilohertz, 0, 'f',
                                          profile.sampleRate.sampleRate % 1000 == 0 ? 0 : 1);
}

} // namespace

QStringList ManualExportPlanner::supportedInputExtensions() {
    // Keep this deliberately master-oriented. It prevents delivery files from
    // becoming implicit directory-export inputs while still allowing manual
    // single-file export of either native master format.
    return {QStringLiteral("wav"), QStringLiteral("flac")};
}

bool ManualExportPlanner::isSupportedInput(const std::filesystem::path& path) {
    const QString extension = QString::fromStdString(path.extension().string())
                                  .trimmed().toLower().remove(QLatin1Char('.'));
    return supportedInputExtensions().contains(extension);
}

QList<ManualExportSource> ManualExportPlanner::enumerateDirectory(
    const std::filesystem::path& directory,
    const bool includeSubdirectories,
    const QList<std::filesystem::path>& excludedDirectories) {
    QList<ManualExportSource> sources;
    const QFileInfo directoryInfo(QString::fromStdString(directory.string()));
    if (!directoryInfo.exists() || !directoryInfo.isDir()) {
        return sources;
    }

    QList<QString> excluded;
    excluded.reserve(excludedDirectories.size() + 1);
    for (const auto& excludedDirectory : excludedDirectories) {
        if (!excludedDirectory.empty()) {
            excluded.append(normalizedPath(excludedDirectory));
        }
    }
    // All generated exports using the standard destination convention live
    // here, so never treat them as new directory-export inputs.
    excluded.append(normalizedPath(directory / "Exports"));

    const auto isExcluded = [&excluded](const std::filesystem::path& candidatePath) {
        const QString normalizedCandidate = normalizedPath(candidatePath);
        bool excludedCandidate = false;
        for (const QString& excludedDirectory : excluded) {
            if (isInsideDirectory(normalizedCandidate, excludedDirectory)) {
                excludedCandidate = true;
                break;
            }
        }
        return excludedCandidate;
    };
    const auto appendIfSupported = [&sources](const std::filesystem::path& candidatePath) {
        if (isSupportedInput(candidatePath)) sources.append(ManualExportSource{.path = candidatePath});
    };

    std::error_code error;
    if (!includeSubdirectories) {
        for (std::filesystem::directory_iterator iterator(
                 directory, std::filesystem::directory_options::skip_permission_denied, error), end;
             !error && iterator != end; iterator.increment(error)) {
            const auto& entry = *iterator;
            if (entry.is_symlink(error) || error) { error.clear(); continue; }
            if (entry.is_regular_file(error) && !error && !isExcluded(entry.path())) {
                appendIfSupported(entry.path());
            }
            error.clear();
        }
    } else {
        for (std::filesystem::recursive_directory_iterator iterator(
                 directory, std::filesystem::directory_options::skip_permission_denied, error), end;
             !error && iterator != end; iterator.increment(error)) {
            const auto& entry = *iterator;
            if (entry.is_symlink(error) || error) { iterator.disable_recursion_pending(); error.clear(); continue; }
            if (isExcluded(entry.path())) { iterator.disable_recursion_pending(); continue; }
            if (entry.is_regular_file(error) && !error) appendIfSupported(entry.path());
            error.clear();
        }
    }
    std::sort(sources.begin(), sources.end(), [](const ManualExportSource& left,
                                                   const ManualExportSource& right) {
        return left.path.string() < right.path.string();
    });
    return sources;
}

bool ManualExportPlanner::planRequest(const ManualExportSource& source,
                                      const ExportProfile& profile,
                                      const ManualExportDestination& destination,
                                      const bool createDestinationDirectories,
                                      ExportRequest* const request,
                                      QString* const error) {
    if (request == nullptr || error == nullptr) {
        return false;
    }
    *error = {};
    if (source.path.empty()) {
        *error = QStringLiteral("Manual export source path is empty");
        return false;
    }
    if (!profile.validate(error)) {
        return false;
    }
    const QFileInfo sourceInfo(QString::fromStdString(source.path.string()));
    if (!sourceInfo.exists() || !sourceInfo.isFile() || sourceInfo.size() == 0) {
        *error = QStringLiteral("Manual export source is missing, not a regular file, or empty");
        return false;
    }

    std::filesystem::path destinationDirectory;
    switch (destination.policy) {
    case ManualExportDestinationPolicy::SameDirectory:
        destinationDirectory = source.path.parent_path();
        break;
    case ManualExportDestinationPolicy::ExportsSubfolder:
        destinationDirectory = source.path.parent_path() / "Exports";
        break;
    case ManualExportDestinationPolicy::CustomDirectory:
        if (destination.customDirectory.trimmed().isEmpty()) {
            *error = QStringLiteral("Choose a custom export directory");
            return false;
        }
        destinationDirectory = destination.customDirectory.toStdString();
        break;
    }

    QString baseName = sourceInfo.completeBaseName();
    if (destination.useStoredMetadataNames && !isPlaceholder(source.trackTitle)) {
        baseName = QString::fromStdString(
            recording::OutputPathPolicy::sanitizePathComponent(source.trackTitle.toStdString()));
    }
    if (baseName.isEmpty()) {
        *error = QStringLiteral("Unable to determine an export filename");
        return false;
    }
    if (destination.useStoredMetadataNames && destination.organizeAlbumDirectory &&
        !isPlaceholder(source.albumTitle)) {
        destinationDirectory /= recording::OutputPathPolicy::sanitizePathComponent(
            source.albumTitle.toStdString());
    }

    if (createDestinationDirectories &&
        !QDir().mkpath(QString::fromStdString(destinationDirectory.string()))) {
        *error = QStringLiteral("Unable to create export destination directory");
        return false;
    }

    *request = {};
    request->sourcePath = source.path;
    request->destinationPath = destinationDirectory /
                               (baseName.toStdString() + "." +
                                exportFileExtension(profile.format).toStdString());
    request->profile = profile;
    request->profile.collisionPolicy = destination.collisionPolicy;
    request->captureSessionId = source.captureSessionId;
    request->sourceSampleRate = source.sampleRate;
    return true;
}

QString ManualExportPlanner::profileSummary(const ExportProfile& profile) {
    QString summary = exportFormatName(profile.format).toUpper() + QStringLiteral(" • ") +
                      sampleRateSummary(profile);
    if (const auto* options = std::get_if<Mp3Options>(&profile.options)) {
        summary += QStringLiteral(" • %1 kbps").arg(options->bitrateKbps);
    } else if (const auto* options = std::get_if<FlacOptions>(&profile.options)) {
        summary += QStringLiteral(" • %1-bit • compression %2")
                       .arg(options->bitDepth).arg(options->compressionLevel);
    } else if (const auto* options = std::get_if<WavOptions>(&profile.options)) {
        switch (options->sampleFormat) {
        case WavSampleFormat::Signed16: summary += QStringLiteral(" • 16-bit PCM"); break;
        case WavSampleFormat::Signed24: summary += QStringLiteral(" • 24-bit PCM"); break;
        case WavSampleFormat::Signed32: summary += QStringLiteral(" • 32-bit PCM"); break;
        case WavSampleFormat::Float32: summary += QStringLiteral(" • 32-bit Float"); break;
        case WavSampleFormat::Unsigned8: summary += QStringLiteral(" • 8-bit PCM"); break;
        }
    }
    return summary;
}

} // namespace abracapture::exporting
