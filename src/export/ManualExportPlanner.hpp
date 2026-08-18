#pragma once

#include "ExportTypes.hpp"

#include <QList>
#include <QString>

#include <cstdint>
#include <filesystem>

namespace abracapture::exporting {

enum class ManualExportDestinationPolicy {
    SameDirectory,
    ExportsSubfolder,
    CustomDirectory,
};

struct ManualExportSource {
    std::filesystem::path path;
    AudioStreamFormat sourceFormat;
    QString captureSessionId;
    QString trackTitle;
    QString albumTitle;
};

struct ManualExportDestination {
    ManualExportDestinationPolicy policy = ManualExportDestinationPolicy::SameDirectory;
    CollisionPolicy collisionPolicy = CollisionPolicy::GenerateUniqueName;
    QString customDirectory;
    bool useStoredMetadataNames = false;
    bool organizeAlbumDirectory = false;
};

class ManualExportPlanner {
public:
    [[nodiscard]] static QStringList supportedInputExtensions();
    [[nodiscard]] static bool isSupportedInput(const std::filesystem::path& path);

    [[nodiscard]] static QList<ManualExportSource> enumerateDirectory(
        const std::filesystem::path& directory,
        bool includeSubdirectories,
        const QList<std::filesystem::path>& excludedDirectories = {});

    [[nodiscard]] static bool planRequest(const ManualExportSource& source,
                                          const ExportProfile& profile,
                                          const ManualExportDestination& destination,
                                          bool createDestinationDirectories,
                                          ExportRequest* request,
                                          QString* error);
    [[nodiscard]] static QString profileSummary(const ExportProfile& profile);
};

} // namespace abracapture::exporting
