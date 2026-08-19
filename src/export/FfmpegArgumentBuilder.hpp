#pragma once

#include "Eac3Capability.hpp"
#include "ExportTypes.hpp"

#include <QString>
#include <QStringList>

#include <optional>

namespace abracapture::exporting {

class FfmpegArgumentBuilder {
public:
    // Backwards-compatible nonblocking entry point. E-AC-3 7.1 is rejected
    // unless the caller supplies the cached capability report below.
    [[nodiscard]] static bool build(const ExportRequest& request,
                                    QStringList* arguments,
                                    QString* error);

    // Preferred entry point: the caller supplies the prepared report from
    // FfmpegCapabilities. It gates 7.1 requests without probing in the
    // export path.
    [[nodiscard]] static bool build(const ExportRequest& request,
                                    const Eac3CapabilityReport& eac3Capability,
                                    QStringList* arguments,
                                    QString* error);
};

} // namespace abracapture::exporting
