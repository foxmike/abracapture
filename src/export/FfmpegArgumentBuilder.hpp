#pragma once

#include "ExportTypes.hpp"

#include <QString>
#include <QStringList>

namespace abracapture::exporting {

class FfmpegArgumentBuilder {
public:
    [[nodiscard]] static bool build(const ExportRequest& request,
                                    QStringList* arguments,
                                    QString* error);
};

} // namespace abracapture::exporting
