#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <spa/param/audio/raw.h>

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
    std::string clientId;
    std::string applicationProcessId;
    std::string applicationProcessBinary;
    std::string objectSerial;
    std::string nodeDescription;
    std::string mediaName;
    std::string mediaTitle;
    std::string mediaAlbum;
    std::string mediaRole;
    std::string logicalIdentity;
    std::string instanceIdentity;
    std::optional<std::uint32_t> deviceId;
    std::optional<std::uint32_t> sampleRate;
    std::optional<std::uint32_t> channels;
    // Current SPA_PARAM_Format reported by this PipeWire node. This is the
    // source-format domain, never a capture-stream negotiation result.
    std::optional<spa_audio_info_raw> sourceFormat;

    bool operator==(const SourceDescriptor& other) const noexcept;
};

SourceCategory classifyMediaClass(std::string_view mediaClass);
bool isCapturableSource(SourceCategory cat);
