#pragma once

#include "ExportTypes.hpp"

#include <QSet>
#include <QString>

namespace abracapture::exporting {

class ExportOutputPlanner {
public:
    [[nodiscard]] static bool plan(const ExportRequest& request,
                                   const QSet<QString>& reservedOutputPaths,
                                   ExportRequest* plannedRequest,
                                   QString* error);

    [[nodiscard]] static QString normalizedPathKey(const std::filesystem::path& path);
};

} // namespace abracapture::exporting
