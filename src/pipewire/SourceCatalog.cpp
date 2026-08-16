#include "SourceCatalog.hpp"

#include "PropertyNormalizer.hpp"

#include <algorithm>
#include <pipewire/keys.h>

const std::vector<SourceDescriptor>& SourceCatalog::sources() const
{
    thread_local std::vector<SourceDescriptor> snapshot;
    {
        std::scoped_lock lock(mutex_);
        snapshot = sources_;
    }
    return snapshot;
}

std::optional<SourceDescriptor> SourceCatalog::sourceById(std::uint32_t id) const
{
    std::scoped_lock lock(mutex_);
    const auto iterator = std::find_if(sources_.begin(), sources_.end(),
                                       [id](const SourceDescriptor& source) {
                                           return source.nodeId == id;
                                       });
    if (iterator == sources_.end())
        return std::nullopt;

    return *iterator;
}

void SourceCatalog::start(PipeWireContext& ctx)
{
    stop();
    context_ = &ctx;

    ctx.globalAdded = [this](std::uint32_t id, const char*, const spa_dict* props) {
        const std::string mediaClass = PropertyNormalizer::extractMediaClass(props);
        const SourceCategory category = classifyMediaClass(mediaClass);
        if (!isCapturableSource(category))
            return;

            SourceDescriptor source{
            .nodeId = id,
            .category = category,
            .displayName = PropertyNormalizer::extractDisplayName(props),
            .appName = PropertyNormalizer::extractAppName(props),
            .mediaClass = mediaClass,
                .deviceId = PropertyNormalizer::extractDeviceId(props),
                .sampleRate = PropertyNormalizer::extractUInt32(props, PW_KEY_AUDIO_RATE),
                .channels = PropertyNormalizer::extractUInt32(props, PW_KEY_AUDIO_CHANNELS),
            };

        bool inserted = false;
        {
            std::scoped_lock lock(mutex_);
            const auto iterator = std::find_if(sources_.begin(), sources_.end(),
                                               [id](const SourceDescriptor& existing) {
                                                   return existing.nodeId == id;
                                               });
            if (iterator == sources_.end()) {
                sources_.push_back(source);
                inserted = true;
            }
        }

        if (inserted && sourceAppeared)
            sourceAppeared(source);
    };

    ctx.globalRemoved = [this](std::uint32_t id) {
        bool removed = false;
        {
            std::scoped_lock lock(mutex_);
            const auto iterator = std::find_if(sources_.begin(), sources_.end(),
                                               [id](const SourceDescriptor& source) {
                                                   return source.nodeId == id;
                                               });
            if (iterator != sources_.end()) {
                sources_.erase(iterator);
                removed = true;
            }
        }

        if (removed && sourceDisappeared)
            sourceDisappeared(id);
    };
}

void SourceCatalog::stop()
{
    if (context_ != nullptr) {
        context_->globalAdded = {};
        context_->globalRemoved = {};
        context_ = nullptr;
    }

    bool wasPopulated = false;
    {
        std::scoped_lock lock(mutex_);
        wasPopulated = !sources_.empty();
        sources_.clear();
    }

    if (wasPopulated && catalogReset)
        catalogReset();
}
