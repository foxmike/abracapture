#pragma once

#include "PipeWireContext.hpp"
#include "SourceDescriptor.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

class SourceCatalog {
public:
    SourceCatalog() = default;

    SourceCatalog(const SourceCatalog&) = delete;
    SourceCatalog& operator=(const SourceCatalog&) = delete;

    const std::vector<SourceDescriptor>& sources() const;
    std::optional<SourceDescriptor> sourceById(std::uint32_t id) const;

    void start(PipeWireContext& ctx);
    void stop();

    std::function<void(const SourceDescriptor&)> sourceAppeared;
    std::function<void(std::uint32_t nodeId)> sourceDisappeared;
    std::function<void()> catalogReset;

private:
    mutable std::mutex mutex_;
    std::vector<SourceDescriptor> sources_;
    PipeWireContext* context_ = nullptr;
};
