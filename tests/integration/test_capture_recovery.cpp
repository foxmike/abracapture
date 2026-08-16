#include "audio/AudioRingBuffer.hpp"
#include "audio/CaptureMetrics.hpp"
#include "audio/PipeWireCaptureStream.hpp"
#include "pipewire/PipeWireContext.hpp"
#include "pipewire/SourceCatalog.hpp"

#include <gtest/gtest.h>

#include <pipewire/context.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <pipewire/properties.h>
#include <pipewire/stream.h>
#include <pipewire/thread-loop.h>
#include <spa/buffer/buffer.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/param.h>
#include <spa/pod/builder.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

// CaptureSession intentionally has no public fault-injection surface. This test
// exposes only its real stream handle so the integration test can disconnect
// the live PipeWire stream and exercise the production status/reconnect
// callbacks.
#define private public
#include "audio/CaptureSession.hpp"
#undef private

namespace {

using namespace std::chrono_literals;
using abracapture::audio::CaptureSession;
using abracapture::audio::SessionState;

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

template <typename Predicate>
bool waitUntil(Predicate &&predicate, const std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (std::invoke(predicate)) {
      return true;
    }
    std::this_thread::sleep_for(10ms);
  }
  return std::invoke(predicate);
}

class TestAudioProducer {
public:
  explicit TestAudioProducer(std::string nodeName)
      : nodeName_(std::move(nodeName)) {}

  ~TestAudioProducer() { stop(); }

  bool start() {
    pw_init(nullptr, nullptr);
    initialized_ = true;
    loop_ = pw_thread_loop_new("abracapture-capture-test-graph", nullptr);
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
    registry_ = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);
    if (registry_ == nullptr) {
      stop();
      return false;
    }

    static const pw_registry_events registryEvents{
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global =
            [](void *data, const std::uint32_t id, std::uint32_t,
               const char *const type, std::uint32_t,
               const spa_dict *const properties) {
              if (type == nullptr || properties == nullptr) {
                return;
              }
              auto &self = *static_cast<TestAudioProducer *>(data);
              if (std::string(type) == PW_TYPE_INTERFACE_Link) {
                const char *const outputNode =
                    spa_dict_lookup(properties, PW_KEY_LINK_OUTPUT_NODE);
                std::lock_guard lock(self.registryMutex_);
                if (outputNode != nullptr &&
                    std::strtoul(outputNode, nullptr, 10) == self.nodeId_) {
                  self.temporaryLinkIds_.insert(id);
                }
                self.registryCondition_.notify_all();
                return;
              }
              if (std::string(type) != PW_TYPE_INTERFACE_Node) {
                return;
              }
              const char *const nodeName =
                  spa_dict_lookup(properties, PW_KEY_NODE_NAME);
              const char *const appName =
                  spa_dict_lookup(properties, PW_KEY_APP_NAME);
              const char *const category =
                  spa_dict_lookup(properties, PW_KEY_MEDIA_CATEGORY);
              {
                std::lock_guard lock(self.registryMutex_);
                if (nodeName != nullptr && self.nodeName_ == nodeName) {
                  self.nodeId_ = id;
                }
                if (appName != nullptr &&
                    std::string(appName) == "Abracapture" &&
                    category != nullptr && std::string(category) == "Capture") {
                  self.captureProxyIds_.insert(id);
                }
              }
              self.registryCondition_.notify_all();
            },
        .global_remove =
            [](void *data, const std::uint32_t id) {
              auto &self = *static_cast<TestAudioProducer *>(data);
              {
                std::lock_guard lock(self.registryMutex_);
                self.captureProxyIds_.erase(id);
                self.temporaryLinkIds_.erase(id);
                if (self.nodeId_ == id) {
                  self.nodeId_ = PW_ID_ANY;
                }
              }
              self.registryCondition_.notify_all();
            },
    };
    if (pw_registry_add_listener(registry_, &registryListener_, &registryEvents,
                                 this) < 0) {
      stop();
      return false;
    }
    registryListenerInstalled_ = true;

    pw_properties *const properties = pw_properties_new(
        PW_KEY_NODE_NAME, nodeName_.c_str(), PW_KEY_NODE_DESCRIPTION,
        "Abracapture generated test audio", PW_KEY_APP_NAME,
        "Abracapture integration producer", PW_KEY_MEDIA_CLASS,
        "Stream/Output/Audio", PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Test",
        PW_KEY_AUDIO_FORMAT, "F32LE", PW_KEY_AUDIO_RATE, "48000",
        PW_KEY_AUDIO_CHANNELS, "2", PW_KEY_NODE_VIRTUAL, "true",
        PW_KEY_NODE_ALWAYS_PROCESS, "true", nullptr);
    stream_ =
        pw_stream_new(core_, "Abracapture generated test audio", properties);
    if (stream_ == nullptr) {
      stop();
      return false;
    }

    static const pw_stream_events streamEvents = [] {
      pw_stream_events events{};
      events.version = PW_VERSION_STREAM_EVENTS;
      events.state_changed = [](void *data, pw_stream_state,
                                pw_stream_state state, const char *) {
        auto &self = *static_cast<TestAudioProducer *>(data);
        if (state == PW_STREAM_STATE_PAUSED ||
            state == PW_STREAM_STATE_STREAMING) {
          {
            std::lock_guard lock(self.stateMutex_);
            self.ready_ = true;
          }
          self.stateCondition_.notify_all();
        }
      };
      events.process = [](void *data) {
        static_cast<TestAudioProducer *>(data)->process();
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
                                                    PW_STREAM_FLAG_MAP_BUFFERS |
                                                    PW_STREAM_FLAG_RT_PROCESS);
    if (pw_stream_connect(stream_, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags,
                          parameters, 1) < 0 ||
        pw_thread_loop_start(loop_) < 0) {
      stop();
      return false;
    }
    loopStarted_ = true;

    {
      std::unique_lock lock(stateMutex_);
      if (!stateCondition_.wait_for(lock, 5s, [this] { return ready_; })) {
        stop();
        return false;
      }
    }
    if (!waitUntil([this] { return nodeId() != PW_ID_ANY; }, 5s)) {
      stop();
      return false;
    }

    triggerRunning_.store(true, std::memory_order_release);
    triggerThread_ = std::thread([this] {
      while (triggerRunning_.load(std::memory_order_acquire)) {
        if (stream_ != nullptr) {
          static_cast<void>(pw_stream_trigger_process(stream_));
        }
        std::this_thread::sleep_for(5ms);
      }
    });
    return true;
  }

  void stop() noexcept {
    triggerRunning_.store(false, std::memory_order_release);
    if (triggerThread_.joinable()) {
      triggerThread_.join();
    }
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
    if (registryListenerInstalled_) {
      spa_hook_remove(&registryListener_);
      registryListenerInstalled_ = false;
    }
    if (registry_ != nullptr) {
      pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry_));
      registry_ = nullptr;
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

  [[nodiscard]] pw_thread_loop *loop() const noexcept { return loop_; }
  [[nodiscard]] pw_core *core() const noexcept { return core_; }

  [[nodiscard]] std::uint32_t nodeId() const noexcept {
    std::lock_guard lock(registryMutex_);
    return nodeId_;
  }

  [[nodiscard]] std::size_t captureProxyCount() const noexcept {
    std::lock_guard lock(registryMutex_);
    return captureProxyIds_.size();
  }

  [[nodiscard]] std::size_t temporaryLinkCount() const noexcept {
    std::lock_guard lock(registryMutex_);
    return temporaryLinkIds_.size();
  }

private:
  void process() noexcept {
    pw_buffer *const pipeWireBuffer = pw_stream_dequeue_buffer(stream_);
    if (pipeWireBuffer == nullptr || pipeWireBuffer->buffer == nullptr ||
        pipeWireBuffer->buffer->n_datas == 0) {
      return;
    }
    spa_data &data = pipeWireBuffer->buffer->datas[0];
    if (data.data != nullptr && data.chunk != nullptr) {
      constexpr std::size_t channels = 2;
      constexpr std::size_t requestedFrames = 256;
      const std::size_t availableFrames =
          data.maxsize / (sizeof(float) * channels);
      const std::size_t frames = std::min(requestedFrames, availableFrames);
      auto *const output = static_cast<float *>(data.data);
      for (std::size_t frame = 0; frame < frames; ++frame) {
        phase_ += 0.03F;
        if (phase_ > 6.2831853F) {
          phase_ -= 6.2831853F;
        }
        const float sample = std::sin(phase_) * 0.25F;
        output[frame * channels] = sample;
        output[frame * channels + 1] = sample;
      }
      data.chunk->offset = 0;
      data.chunk->stride = static_cast<std::int32_t>(sizeof(float) * channels);
      data.chunk->size =
          static_cast<std::uint32_t>(frames * sizeof(float) * channels);
      pipeWireBuffer->size = frames;
    }
    static_cast<void>(pw_stream_queue_buffer(stream_, pipeWireBuffer));
  }

  std::string nodeName_;
  pw_thread_loop *loop_ = nullptr;
  pw_context *context_ = nullptr;
  pw_core *core_ = nullptr;
  pw_registry *registry_ = nullptr;
  pw_stream *stream_ = nullptr;
  spa_hook registryListener_{};
  spa_hook streamListener_{};
  bool initialized_ = false;
  bool loopStarted_ = false;
  bool registryListenerInstalled_ = false;
  bool streamListenerInstalled_ = false;

  mutable std::mutex registryMutex_;
  std::condition_variable registryCondition_;
  std::uint32_t nodeId_ = PW_ID_ANY;
  std::unordered_set<std::uint32_t> captureProxyIds_;
  std::unordered_set<std::uint32_t> temporaryLinkIds_;
  std::mutex stateMutex_;
  std::condition_variable stateCondition_;
  bool ready_ = false;
  std::atomic_bool triggerRunning_{false};
  std::thread triggerThread_;
  float phase_ = 0.0F;
};

bool waitForSource(const SourceCatalog &catalog, const std::uint32_t nodeId) {
  return waitUntil(
      [&catalog, nodeId] { return catalog.sourceById(nodeId).has_value(); },
      5s);
}

} // namespace

TEST(CaptureRecoveryIntegration,
     ReceivesFramesAndReportsBoundedRecoveryFailure) {
  HardTimeoutGuard timeout(30s);
  PipeWireContext registryContext;
  SourceCatalog catalog;
  catalog.start(registryContext);
  ASSERT_TRUE(registryContext.start());

  TestAudioProducer producer("abracapture.capture.recovery.source");
  ASSERT_TRUE(producer.start());
  const std::uint32_t sourceId = producer.nodeId();
  ASSERT_NE(sourceId, PW_ID_ANY);
  ASSERT_TRUE(waitForSource(catalog, sourceId));

  std::atomic_bool sawReconnecting{false};
  std::atomic_bool sawError{false};
  std::atomic<std::size_t> consumedSamples{0};
  const std::size_t descriptorBaseline = openFileDescriptorCount();
  const std::size_t proxyBaseline = producer.captureProxyCount();
  const std::size_t linkBaseline = producer.temporaryLinkCount();
  {
    CaptureSession session(
        producer.loop(), producer.core(), catalog, 65536,
        [&](const SessionState state) {
          if (state == SessionState::Reconnecting) {
            sawReconnecting.store(true, std::memory_order_release);
          }
          if (state == SessionState::Error) {
            sawError.store(true, std::memory_order_release);
          }
        },
        [&](const float *, const std::size_t samples) {
          consumedSamples.fetch_add(samples, std::memory_order_relaxed);
        });
    session.setMaxReconnectAttempts(1);
    ASSERT_TRUE(session.begin(sourceId, SPA_AUDIO_FORMAT_F32, 48000, 2));
    ASSERT_TRUE(waitUntil(
        [&session] { return session.state() == SessionState::Capturing; }, 5s));

    CaptureSession::CaptureVisualizationSnapshot snapshot;
    ASSERT_TRUE(
        waitUntil([&] { return session.visualizationSnapshot(snapshot); }, 5s));
    EXPECT_GT(snapshot.sampleCount, 0U);

    ASSERT_TRUE(static_cast<bool>(registryContext.globalRemoved));
    registryContext.globalRemoved(sourceId);
    ASSERT_TRUE(!catalog.sourceById(sourceId).has_value());

    const auto recoveryStarted = std::chrono::steady_clock::now();
    {
      std::lock_guard lock(session.streamMutex_);
      ASSERT_NE(session.stream_, nullptr);
      session.stream_->stop();
    }
    ASSERT_TRUE(waitUntil(
        [&sawReconnecting] {
          return sawReconnecting.load(std::memory_order_acquire);
        },
        1s));
    ASSERT_TRUE(waitUntil(
        [&sawError] { return sawError.load(std::memory_order_acquire); }, 3s));
    EXPECT_EQ(session.state(), SessionState::Error);
    EXPECT_TRUE(std::chrono::steady_clock::now() - recoveryStarted <= 3s);

    session.stop();
    EXPECT_EQ(session.state(), SessionState::Stopped);
    EXPECT_GT(consumedSamples.load(std::memory_order_acquire), 0U);
  }
  ASSERT_TRUE(waitUntil(
      [&] {
        return producer.captureProxyCount() == proxyBaseline &&
               producer.temporaryLinkCount() == linkBaseline;
      },
      2s));
  EXPECT_EQ(openFileDescriptorCount(), descriptorBaseline);

  producer.stop();
  catalog.stop();
  registryContext.stop();
}

TEST(CaptureRecoveryIntegration,
     RapidStartStopReleasesDescriptorsAndPipeWireObjects) {
  HardTimeoutGuard timeout(30s);
  PipeWireContext registryContext;
  SourceCatalog catalog;
  catalog.start(registryContext);
  ASSERT_TRUE(registryContext.start());

  TestAudioProducer producer("abracapture.capture.stress.source");
  ASSERT_TRUE(producer.start());
  const std::uint32_t sourceId = producer.nodeId();
  ASSERT_NE(sourceId, PW_ID_ANY);
  ASSERT_TRUE(waitForSource(catalog, sourceId));

  const std::size_t descriptorBaseline = openFileDescriptorCount();
  const std::size_t proxyBaseline = producer.captureProxyCount();
  const std::size_t linkBaseline = producer.temporaryLinkCount();
  {
    CaptureSession session(producer.loop(), producer.core(), catalog);
    for (std::size_t iteration = 0; iteration < 10; ++iteration) {
      ASSERT_TRUE(session.begin(sourceId, SPA_AUDIO_FORMAT_F32, 48000, 2));
      ASSERT_TRUE(waitUntil(
          [&session] { return session.state() == SessionState::Capturing; },
          1s));
      session.stop();
      EXPECT_EQ(session.state(), SessionState::Stopped);
      ASSERT_TRUE(waitUntil(
          [&] {
            return producer.captureProxyCount() == proxyBaseline &&
                   producer.temporaryLinkCount() == linkBaseline;
          },
          1s));
      EXPECT_EQ(openFileDescriptorCount(), descriptorBaseline);
      EXPECT_EQ(producer.captureProxyCount(), proxyBaseline);
      EXPECT_EQ(producer.temporaryLinkCount(), linkBaseline);
    }

    session.stop();
  }

  producer.stop();
  catalog.stop();
  registryContext.stop();
}
