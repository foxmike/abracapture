#include "SourceDescriptor.hpp"

SourceCategory classifyMediaClass(std::string_view mediaClass)
{
    if (mediaClass == "Stream/Output/Audio" || mediaClass == "Stream/Output/Video") {
        return SourceCategory::ApplicationPlayback;
    }

    if (mediaClass == "Audio/Sink") {
        return SourceCategory::OutputMonitor;
    }

    if (mediaClass == "Audio/Source" || mediaClass == "Audio/Source/Virtual") {
        return SourceCategory::HardwareInput;
    }

    return SourceCategory::Other;
}

bool isCapturableSource(SourceCategory cat)
{
    return cat == SourceCategory::ApplicationPlayback ||
           cat == SourceCategory::OutputMonitor ||
           cat == SourceCategory::HardwareInput;
}

bool SourceDescriptor::operator==(const SourceDescriptor& other) const noexcept
{
    return nodeId == other.nodeId;
}
