#include "SourceCatalog.hpp"

#include "PropertyNormalizer.hpp"

#include <algorithm>
#include <cstdio>
#include <pipewire/keys.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/param.h>

namespace {

std::string property(const spa_dict* const props, const char* const key)
{
    const char* const value = props == nullptr ? nullptr : spa_dict_lookup(props, key);
    return value == nullptr ? std::string{} : PropertyNormalizer::sanitizeUtf8(value);
}

std::string bounded(const std::string& value)
{
    constexpr std::size_t maxLength = 120;
    if (value.size() <= maxLength)
        return value;
    return value.substr(0, maxLength - 3) + "...";
}

bool isApplicationPlayback(const SourceDescriptor& source)
{
    return source.category == SourceCategory::ApplicationPlayback;
}

void logSourceEvent(const char* const event, const SourceDescriptor& source)
{
    if (!isApplicationPlayback(source))
        return;

    std::fprintf(stderr,
                 "[abracapture] source event=%s node.id=%u object.serial=%s client.id=%s "
                 "application.name=%s application.process.id=%s "
                 "application.process.binary=%s node.name=%s node.description=%s "
                 "media.class=%s media.name=%s media.title=%s media.album=%s "
                 "media.role=%s logical-key=%s instance-key=%s\n",
                 event,
                 source.nodeId,
                 bounded(source.objectSerial).c_str(),
                 bounded(source.clientId).c_str(),
                 bounded(source.appName).c_str(),
                 bounded(source.applicationProcessId).c_str(),
                 bounded(source.applicationProcessBinary).c_str(),
                 bounded(source.nodeName).c_str(),
                 bounded(source.nodeDescription).c_str(),
                 bounded(source.mediaClass).c_str(),
                 bounded(source.mediaName).c_str(),
                 bounded(source.mediaTitle).c_str(),
                 bounded(source.mediaAlbum).c_str(),
                 bounded(source.mediaRole).c_str(),
                 bounded(source.logicalIdentity).c_str(),
                 bounded(source.instanceIdentity).c_str());
}

void updateNodeMetadata(SourceDescriptor& source, const spa_dict* const props)
{
    if (props == nullptr)
        return;

    const std::string mediaClass = PropertyNormalizer::extractMediaClass(props);
    const std::string appName = PropertyNormalizer::extractAppName(props);
    const std::string nodeName = PropertyNormalizer::extractNodeName(props);
    const std::string clientId = property(props, PW_KEY_CLIENT_ID);
    const std::string processId = property(props, PW_KEY_APP_PROCESS_ID);
    const std::string processBinary = property(props, PW_KEY_APP_PROCESS_BINARY);
    const std::string objectSerial = property(props, PW_KEY_OBJECT_SERIAL);
    const std::string nodeDescription = property(props, PW_KEY_NODE_DESCRIPTION);
    const std::string mediaName = property(props, PW_KEY_MEDIA_NAME);
    const std::string mediaTitle = property(props, PW_KEY_MEDIA_TITLE);
    const std::string mediaAlbum = property(props, "media.album");
    const std::string mediaRole = property(props, PW_KEY_MEDIA_ROLE);

    if (!mediaClass.empty()) source.mediaClass = mediaClass;
    if (!appName.empty()) source.appName = appName;
    if (!nodeName.empty()) source.nodeName = nodeName;
    if (!clientId.empty()) source.clientId = clientId;
    if (!processId.empty()) source.applicationProcessId = processId;
    if (!processBinary.empty()) source.applicationProcessBinary = processBinary;
    if (!objectSerial.empty()) source.objectSerial = objectSerial;
    if (!nodeDescription.empty()) source.nodeDescription = nodeDescription;
    if (!mediaName.empty()) source.mediaName = mediaName;
    if (!mediaTitle.empty()) source.mediaTitle = mediaTitle;
    if (!mediaAlbum.empty()) source.mediaAlbum = mediaAlbum;
    if (!mediaRole.empty()) source.mediaRole = mediaRole;
    source.displayName = PropertyNormalizer::extractDisplayName(props);

    // client.id is present in the registry announcement and remains stable
    // across node recreation. Process metadata is diagnostic-only here because
    // it arrives later through node info and would otherwise change the key in
    // place.
    const std::string stableGroup = source.mediaClass + "\x1f" + source.clientId +
                                    "\x1f" + source.appName + "\x1f" + source.nodeName;
    source.logicalIdentity = stableGroup;
    const std::string instanceSuffix = source.mediaName.empty()
                                           ? source.objectSerial
                                           : source.mediaName + "\x1f" + source.objectSerial;
    source.instanceIdentity = stableGroup + "\x1f" + instanceSuffix;
    if (const auto deviceId = PropertyNormalizer::extractDeviceId(props))
        source.deviceId = deviceId;
}

bool nodeMetadataChanged(const SourceDescriptor& before, const SourceDescriptor& after)
{
    return before.displayName != after.displayName || before.appName != after.appName ||
           before.mediaClass != after.mediaClass || before.nodeName != after.nodeName ||
           before.clientId != after.clientId ||
           before.applicationProcessId != after.applicationProcessId ||
           before.applicationProcessBinary != after.applicationProcessBinary ||
           before.objectSerial != after.objectSerial ||
           before.nodeDescription != after.nodeDescription ||
           before.mediaName != after.mediaName || before.mediaTitle != after.mediaTitle ||
           before.mediaAlbum != after.mediaAlbum || before.mediaRole != after.mediaRole ||
           before.logicalIdentity != after.logicalIdentity ||
           before.instanceIdentity != after.instanceIdentity || before.deviceId != after.deviceId;
}

} // namespace

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

        const std::string appName = PropertyNormalizer::extractAppName(props);
        const std::string nodeName = PropertyNormalizer::extractNodeName(props);
        const std::string clientId = property(props, PW_KEY_CLIENT_ID);
        const std::string processId = property(props, PW_KEY_APP_PROCESS_ID);
        const std::string processBinary = property(props, PW_KEY_APP_PROCESS_BINARY);
        const std::string objectSerial = property(props, PW_KEY_OBJECT_SERIAL);
        const std::string mediaName = property(props, PW_KEY_MEDIA_NAME);
        const std::string mediaTitle = property(props, PW_KEY_MEDIA_TITLE);
        const std::string mediaAlbum = property(props, "media.album");
        const std::string mediaRole = property(props, PW_KEY_MEDIA_ROLE);
        const std::string stableGroup = mediaClass + "\x1f" + clientId + "\x1f" +
                                        appName + "\x1f" + nodeName;
        // media.name distinguishes simultaneous PulseAudio streams belonging
        // to the same client. It is deliberately an instance discriminator,
        // never part of the persistent logical selection group.
        const std::string instanceSuffix = mediaName.empty()
                                                ? objectSerial
                                                : mediaName + "\x1f" + objectSerial;
        const std::string instanceKey = stableGroup + "\x1f" + instanceSuffix;

        SourceDescriptor source{
            .nodeId = id,
            .category = category,
            .displayName = PropertyNormalizer::extractDisplayName(props),
            .appName = appName,
            .mediaClass = mediaClass,
            .nodeName = nodeName,
            .clientId = clientId,
            .applicationProcessId = processId,
            .applicationProcessBinary = processBinary,
            .objectSerial = objectSerial,
            .nodeDescription = property(props, PW_KEY_NODE_DESCRIPTION),
            .mediaName = mediaName,
            .mediaTitle = mediaTitle,
            .mediaAlbum = mediaAlbum,
            .mediaRole = mediaRole,
            .logicalIdentity = stableGroup,
            .instanceIdentity = instanceKey,
            .deviceId = PropertyNormalizer::extractDeviceId(props),
            };

        logSourceEvent("add", source);

        bool inserted = false;
        bool replaced = false;
        {
            std::scoped_lock lock(mutex_);
            const auto iterator = std::find_if(sources_.begin(), sources_.end(),
                                               [id](const SourceDescriptor& existing) {
                                                   return existing.nodeId == id;
                                               });
            if (iterator == sources_.end()) {
                sources_.push_back(source);
                inserted = true;
            } else if (iterator->objectSerial != source.objectSerial) {
                *iterator = source;
                replaced = true;
            }
        }

        if (inserted && sourceAppeared)
            sourceAppeared(source);
        else if (replaced && sourceChanged) {
            logSourceEvent("replacement", source);
            sourceChanged(source);
        }
    };

    ctx.globalRemoved = [this](std::uint32_t id) {
        bool removed = false;
        std::optional<SourceDescriptor> removedSource;
        {
            std::scoped_lock lock(mutex_);
            const auto iterator = std::find_if(sources_.begin(), sources_.end(),
                                               [id](const SourceDescriptor& source) {
                                                   return source.nodeId == id;
                                               });
            if (iterator != sources_.end()) {
                removedSource = *iterator;
                sources_.erase(iterator);
                removed = true;
            }
        }

        if (removedSource)
            logSourceEvent("remove", *removedSource);

        if (removed && sourceDisappeared)
            sourceDisappeared(id);
    };

    ctx.nodePropertiesChanged = [this](const std::uint32_t id, const spa_dict* const props) {
        std::optional<SourceDescriptor> changed;
        {
            std::scoped_lock lock(mutex_);
            const auto iterator = std::find_if(sources_.begin(), sources_.end(),
                                               [id](const SourceDescriptor& source) {
                                                   return source.nodeId == id;
                                               });
            if (iterator == sources_.end() || !isCapturableSource(iterator->category))
                return;

            const SourceDescriptor before = *iterator;
            updateNodeMetadata(*iterator, props);
            if (nodeMetadataChanged(before, *iterator))
                changed = *iterator;
        }
        if (changed) {
            logSourceEvent("update", *changed);
            if (sourceChanged)
                sourceChanged(*changed);
        }
    };

    ctx.nodeFormatChanged = [this](const std::uint32_t id, const spa_pod* const param) {
        spa_audio_info_raw format{};
        if (spa_format_audio_raw_parse(param, &format) < 0 ||
            format.format == SPA_AUDIO_FORMAT_UNKNOWN || format.rate == 0 ||
            format.channels == 0 || format.channels > SPA_AUDIO_MAX_CHANNELS) {
            return;
        }

        std::optional<SourceDescriptor> changed;
        {
            std::scoped_lock lock(mutex_);
            const auto iterator = std::find_if(sources_.begin(), sources_.end(),
                                               [id](const SourceDescriptor& source) {
                                                   return source.nodeId == id;
                                               });
            if (iterator == sources_.end()) {
                return;
            }
            const bool same = iterator->sourceFormat &&
                              iterator->sourceFormat->format == format.format &&
                              iterator->sourceFormat->rate == format.rate &&
                              iterator->sourceFormat->channels == format.channels &&
                              std::equal(iterator->sourceFormat->position,
                                         iterator->sourceFormat->position + format.channels,
                                         format.position);
            if (same) {
                return;
            }
            iterator->sourceFormat = format;
            iterator->sampleRate = format.rate;
            iterator->channels = format.channels;
            changed = *iterator;
        }
        if (changed)
            logSourceEvent("update", *changed);
        if (changed && sourceChanged) {
            sourceChanged(*changed);
        }
    };
}

void SourceCatalog::stop()
{
    if (context_ != nullptr) {
        context_->globalAdded = {};
        context_->globalRemoved = {};
        context_->nodePropertiesChanged = {};
        context_->nodeFormatChanged = {};
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
