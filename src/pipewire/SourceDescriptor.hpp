#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

enum class SourceCategory {
    ApplicationPlayback,
    OutputMonitor,
    HardwareInput,
    Other,
};

struct SourceDescriptor {
    std::uint32_t nodeId = 0;
    SourceCategory category = SourceCategory::Other;
    std::string displayName;
    std::string appName;
    std::string mediaClass;
    std::string nodeName;
    std::optional<std::uint32_t> deviceId;
    std::optional<std::uint32_t> sampleRate;
    std::optional<std::uint32_t> channels;

    bool operator==(const SourceDescriptor& other) const noexcept;
};

SourceCategory classifyMediaClass(std::string_view mediaClass);
bool isCapturableSource(SourceCategory cat);
