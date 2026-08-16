#include "pipewire/PipeWireContext.hpp"

#include <gtest/gtest.h>

#include <pipewire/context.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <pipewire/properties.h>
#include <pipewire/stream.h>
#include <pipewire/thread-loop.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/param.h>
#include <spa/pod/builder.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace {

using namespace std::chrono_literals;

class HardTimeoutGuard {
public:
  explicit HardTimeoutGuard(const std::chrono::seconds timeout)
      : watchdog_([this, timeout] {
          std::unique_lock lock(mutex_);
          if (!condition_.wait_for(lock, timeout,
                                   [this] { return finished_; })) {
            std::abort();
          }
        }) {}

  ~HardTimeoutGuard() {
    {
      std::lock_guard lock(mutex_);
      finished_ = true;
    }
    condition_.notify_one();
    watchdog_.join();
  }

  HardTimeoutGuard(const HardTimeoutGuard &) = delete;
  HardTimeoutGuard &operator=(const HardTimeoutGuard &) = delete;

private:
  std::mutex mutex_;
  std::condition_variable condition_;
  bool finished_ = false;
  std::thread watchdog_;
};

std::size_t openFileDescriptorCount() {
  std::size_t count = 0;
  for (const auto &entry :
       std::filesystem::directory_iterator("/proc/self/fd")) {
    static_cast<void>(entry);
    ++count;
  }
  return count;
}

bool isAudioEndpoint(const char *const mediaClass) {
  if (mediaClass == nullptr) {
    return false;
  }
  const std::string value(mediaClass);
  return value == "Audio/Sink" || value == "Audio/Source" ||
         value == "Audio/Source/Virtual";
}

const char *firstProperty(const spa_dict *const properties,
                          const char *const first,
                          const char *const second = nullptr) {
  if (properties == nullptr) {
    return nullptr;
  }
  if (const char *const value = spa_dict_lookup(properties, first)) {
    return value;
  }
  return second == nullptr ? nullptr : spa_dict_lookup(properties, second);
}

struct ObservedNode {
  std::uint32_t id = PW_ID_ANY;
  std::string name;
  std::string mediaClass;
  std::string format;
};

class SyntheticAudioNode {
public:
  explicit SyntheticAudioNode(std::string nodeName)
      : nodeName_(std::move(nodeName)) {}

  ~SyntheticAudioNode() { stop(); }

  bool start() {
    pw_init(nullptr, nullptr);
    initialized_ = true;

    loop_ = pw_thread_loop_new("abracapture-registry-test-node", nullptr);
    if (loop_ == nullptr) {
      stop();
      return false;
    }
    context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
    if (context_ == nullptr) {
      stop();
      return false;
    }
    core_ = pw_context_connect(context_, nullptr, 0);
    if (core_ == nullptr) {
      stop();
      return false;
    }

    pw_properties *const properties = pw_properties_new(
        PW_KEY_NODE_NAME, nodeName_.c_str(), PW_KEY_NODE_DESCRIPTION,
        "Abracapture registry integration node", PW_KEY_MEDIA_CLASS,
        "Audio/Source/Virtual", PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Test",
        PW_KEY_AUDIO_FORMAT, "F32LE", PW_KEY_AUDIO_RATE, "48000",
        PW_KEY_AUDIO_CHANNELS, "2", PW_KEY_NODE_VIRTUAL, "true",
        PW_KEY_NODE_ALWAYS_PROCESS, "true", nullptr);
    stream_ = pw_stream_new(core_, "Abracapture registry integration node",
                            properties);
    if (stream_ == nullptr) {
      stop();
      return false;
    }

    static const pw_stream_events streamEvents = [] {
      pw_stream_events events{};
      events.version = PW_VERSION_STREAM_EVENTS;
      events.state_changed = [](void *data, pw_stream_state,
                                pw_stream_state state, const char *) {
        auto &self = *static_cast<SyntheticAudioNode *>(data);
        if (state == PW_STREAM_STATE_PAUSED ||
            state == PW_STREAM_STATE_STREAMING) {
          {
            std::lock_guard lock(self.mutex_);
            self.ready_ = true;
          }
          self.condition_.notify_all();
        }
      };
      return events;
    }();
    pw_stream_add_listener(stream_, &streamListener_, &streamEvents, this);
    streamListenerInstalled_ = true;

    alignas(8) std::byte podStorage[1024]{};
    spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(podStorage, sizeof(podStorage));
    spa_audio_info_raw audioInfo{};
    audioInfo.format = SPA_AUDIO_FORMAT_F32;
    audioInfo.rate = 48000;
    audioInfo.channels = 2;
    audioInfo.flags = SPA_AUDIO_FLAG_UNPOSITIONED;
    const spa_pod *parameters[]{
        spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audioInfo)};
    const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_DRIVER |
                                                    PW_STREAM_FLAG_MAP_BUFFERS);
    if (pw_stream_connect(stream_, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags,
                          parameters, 1) < 0 ||
        pw_thread_loop_start(loop_) < 0) {
      stop();
      return false;
    }
    loopStarted_ = true;

    std::unique_lock lock(mutex_);
    return condition_.wait_for(lock, 5s, [this] { return ready_; });
  }

  void stop() noexcept {
    if (loop_ != nullptr && loopStarted_) {
      pw_thread_loop_stop(loop_);
      loopStarted_ = false;
    }
    if (loop_ != nullptr) {
      pw_thread_loop_lock(loop_);
    }
    if (stream_ != nullptr) {
      static_cast<void>(pw_stream_disconnect(stream_));
      if (streamListenerInstalled_) {
        spa_hook_remove(&streamListener_);
        streamListenerInstalled_ = false;
      }
      pw_stream_destroy(stream_);
      stream_ = nullptr;
    }
    if (core_ != nullptr) {
      pw_core_disconnect(core_);
      core_ = nullptr;
    }
    if (context_ != nullptr) {
      pw_context_destroy(context_);
      context_ = nullptr;
    }
    if (loop_ != nullptr) {
      pw_thread_loop_unlock(loop_);
      pw_thread_loop_destroy(loop_);
      loop_ = nullptr;
    }
    if (initialized_) {
      pw_deinit();
      initialized_ = false;
    }
  }

private:
  std::string nodeName_;
  pw_thread_loop *loop_ = nullptr;
  pw_context *context_ = nullptr;
  pw_core *core_ = nullptr;
  pw_stream *stream_ = nullptr;
  spa_hook streamListener_{};
  bool initialized_ = false;
  bool loopStarted_ = false;
  bool streamListenerInstalled_ = false;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool ready_ = false;
};

} // namespace

TEST(PipeWireRegistryIntegration,
     EnumeratesAudioEndpointPropertiesAndDisconnects) {
  HardTimeoutGuard timeout(20s);
  const std::size_t descriptorsBefore = openFileDescriptorCount();

  {
    PipeWireContext context;
    std::mutex mutex;
    std::condition_variable condition;
    ObservedNode endpoint;

    context.globalAdded = [&](const std::uint32_t id, const char *const type,
                              const spa_dict *const properties) {
      if (type == nullptr || std::string(type) != PW_TYPE_INTERFACE_Node) {
        return;
      }
      const char *const mediaClass =
          firstProperty(properties, PW_KEY_MEDIA_CLASS);
      if (!isAudioEndpoint(mediaClass)) {
        return;
      }
      const char *const name =
          firstProperty(properties, PW_KEY_NODE_NAME, PW_KEY_NODE_DESCRIPTION);
      const char *const format =
          firstProperty(properties, PW_KEY_AUDIO_FORMAT, PW_KEY_FORMAT_DSP);
      if (name == nullptr || format == nullptr) {
        return;
      }
      {
        std::lock_guard lock(mutex);
        endpoint = ObservedNode{id, name, mediaClass, format};
      }
      condition.notify_all();
    };

    ASSERT_TRUE(context.start());
    {
      std::unique_lock lock(mutex);
      ASSERT_TRUE(condition.wait_for(
          lock, 8s, [&endpoint] { return endpoint.id != PW_ID_ANY; }));
    }
    context.stop();
    EXPECT_FALSE(context.isRunning());
    ObservedNode observed;
    {
      std::lock_guard lock(mutex);
      observed = endpoint;
    }
    EXPECT_NE(observed.id, PW_ID_ANY);
    EXPECT_FALSE(observed.name.empty());
    EXPECT_TRUE(isAudioEndpoint(observed.mediaClass.c_str()));
    EXPECT_FALSE(observed.format.empty());

    context.globalAdded = {};
    context.globalRemoved = {};
  }

  EXPECT_EQ(openFileDescriptorCount(), descriptorsBefore);
}

TEST(PipeWireRegistryIntegration,
     ChangeCallbackObservesSyntheticNodeAndDeregisters) {
  HardTimeoutGuard timeout(20s);
  const std::size_t descriptorsBefore = openFileDescriptorCount();
  const std::string nodeName =
      "abracapture.registry.integration." + std::to_string(descriptorsBefore);

  {
    PipeWireContext context;
    std::mutex mutex;
    std::condition_variable condition;
    std::uint32_t addedId = PW_ID_ANY;
    std::uint32_t removedId = PW_ID_ANY;
    std::size_t callbackCount = 0;

    context.globalAdded = [&](const std::uint32_t id, const char *,
                              const spa_dict *const properties) {
      const char *const observedName =
          firstProperty(properties, PW_KEY_NODE_NAME);
      if (observedName != nullptr && nodeName == observedName) {
        {
          std::lock_guard lock(mutex);
          addedId = id;
          ++callbackCount;
        }
        condition.notify_all();
      }
    };
    context.globalRemoved = [&](const std::uint32_t id) {
      std::lock_guard lock(mutex);
      if (id == addedId) {
        removedId = id;
        condition.notify_all();
      }
    };

    ASSERT_TRUE(context.start());
    SyntheticAudioNode syntheticNode(nodeName);
    ASSERT_TRUE(syntheticNode.start());
    {
      std::unique_lock lock(mutex);
      ASSERT_TRUE(condition.wait_for(
          lock, 5s, [&addedId] { return addedId != PW_ID_ANY; }));
    }
    EXPECT_EQ(callbackCount, 1U);

    syntheticNode.stop();
    {
      std::unique_lock lock(mutex);
      ASSERT_TRUE(
          condition.wait_for(lock, 5s, [&] { return removedId == addedId; }));
    }

    context.stop();
    context.globalAdded = {};
    context.globalRemoved = {};
    EXPECT_FALSE(static_cast<bool>(context.globalAdded));
    EXPECT_FALSE(static_cast<bool>(context.globalRemoved));
  }

  EXPECT_EQ(openFileDescriptorCount(), descriptorsBefore);
}
