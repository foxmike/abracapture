#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <unordered_map>

#include <pipewire/core.h>
#include <pipewire/node.h>
#include <pipewire/proxy.h>
#include <pipewire/thread-loop.h>
#include <spa/utils/hook.h>

struct pw_context;

class PipeWireContext {
public:
    struct GlobalObject {
        std::uint32_t id = 0;
        const char* type = nullptr;
        std::uint32_t version = 0;
        pw_proxy* proxy = nullptr;
        spa_hook listener{};
        spa_hook nodeListener{};
        bool nodeListenerInstalled = false;
        PipeWireContext* owner = nullptr;
    };

    PipeWireContext() = default;
    ~PipeWireContext();

    PipeWireContext(const PipeWireContext&) = delete;
    PipeWireContext& operator=(const PipeWireContext&) = delete;
    PipeWireContext(PipeWireContext&&) = delete;
    PipeWireContext& operator=(PipeWireContext&&) = delete;

    bool start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] pw_thread_loop* threadLoop() const noexcept { return threadLoop_; }
    [[nodiscard]] pw_core* core() const noexcept { return core_; }

    std::function<void(std::uint32_t id, const char* type, const spa_dict* props)> globalAdded;
    std::function<void(std::uint32_t id)> globalRemoved;
    std::function<void(std::uint32_t id, const spa_dict* props)> nodePropertiesChanged;
    std::function<void(std::uint32_t id, const spa_pod* format)> nodeFormatChanged;

private:
    pw_thread_loop* threadLoop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_registry* registry_ = nullptr;

    spa_hook coreListener_{};
    spa_hook registryListener_{};
    std::unordered_map<std::uint32_t, GlobalObject> globalObjects_;

    std::atomic_bool running_ = false;
    std::atomic_bool connectionAlive_ = false;
    int initialSyncSequence_ = -1;
    bool pipeWireInitialized_ = false;
    bool loopStarted_ = false;
    bool coreListenerInstalled_ = false;
    bool registryListenerInstalled_ = false;
};
