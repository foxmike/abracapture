#include "PipeWireContext.hpp"

#include <pipewire/context.h>
#include <pipewire/pipewire.h>

namespace {

const pw_proxy_events proxyEvents{
    .version = PW_VERSION_PROXY_EVENTS,
    .destroy = [](void* data) {
        auto& object = *static_cast<PipeWireContext::GlobalObject*>(data);
        object.proxy = nullptr;
        object.type = nullptr;
    },
};

} // namespace

PipeWireContext::~PipeWireContext() {
    stop();
}

bool PipeWireContext::start() {
    if (threadLoop_ != nullptr) {
        return isRunning();
    }

    pw_init(nullptr, nullptr);
    pipeWireInitialized_ = true;

    threadLoop_ = pw_thread_loop_new("abracapture-pipewire", nullptr);
    if (threadLoop_ == nullptr) {
        stop();
        return false;
    }

    context_ = pw_context_new(pw_thread_loop_get_loop(threadLoop_), nullptr, 0);
    if (context_ == nullptr) {
        stop();
        return false;
    }

    core_ = pw_context_connect(context_, nullptr, 0);
    if (core_ == nullptr) {
        stop();
        return false;
    }

    static const pw_core_events coreEvents{
        .version = PW_VERSION_CORE_EVENTS,
        .done = [](void* data, std::uint32_t id, int sequence) {
            auto& self = *static_cast<PipeWireContext*>(data);
            if (id == PW_ID_CORE && sequence == self.initialSyncSequence_) {
                self.connectionAlive_.store(true, std::memory_order_release);
            }
        },
        .error = [](void* data, std::uint32_t id, int, int result, const char*) {
            auto& self = *static_cast<PipeWireContext*>(data);
            if (id == PW_ID_CORE && result < 0) {
                self.connectionAlive_.store(false, std::memory_order_release);
                self.running_.store(false, std::memory_order_release);
            }
        },
    };

    if (pw_core_add_listener(core_, &coreListener_, &coreEvents, this) < 0) {
        stop();
        return false;
    }
    coreListenerInstalled_ = true;

    registry_ = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);
    if (registry_ == nullptr) {
        stop();
        return false;
    }

    try {
        globalObjects_.reserve(256);
    } catch (...) {
        stop();
        return false;
    }

    static const pw_registry_events registryEvents{
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = [](void* data, std::uint32_t id, std::uint32_t, const char* type,
                     std::uint32_t version, const spa_dict* props) {
            auto& self = *static_cast<PipeWireContext*>(data);
            const bool lockNeeded = !pw_thread_loop_in_thread(self.threadLoop_);
            if (lockNeeded) {
                pw_thread_loop_lock(self.threadLoop_);
            }

            pw_proxy* proxy = static_cast<pw_proxy*>(
                pw_registry_bind(self.registry_, id, type, version, 0));

            if (proxy != nullptr) {
                try {
                    std::uint32_t proxyVersion = version;
                    auto [iterator, inserted] = self.globalObjects_.try_emplace(id);
                    GlobalObject& object = iterator->second;

                    if (!inserted && object.proxy != nullptr) {
                        spa_hook_remove(&object.listener);
                        pw_proxy_destroy(object.proxy);
                    }

                    object = GlobalObject{
                        .id = id,
                        .type = pw_proxy_get_type(proxy, &proxyVersion),
                        .version = version,
                        .proxy = proxy,
                    };
                    pw_proxy_add_listener(proxy, &object.listener, &proxyEvents, &object);

                    try {
                        if (self.globalAdded) {
                            self.globalAdded(id, object.type, props);
                        }
                    } catch (...) {
                        // Exceptions must not cross the PipeWire C callback boundary.
                    }
                } catch (...) {
                    pw_proxy_destroy(proxy);
                }
            }

            if (lockNeeded) {
                pw_thread_loop_unlock(self.threadLoop_);
            }
        },
        .global_remove = [](void* data, std::uint32_t id) {
            auto& self = *static_cast<PipeWireContext*>(data);
            const bool lockNeeded = !pw_thread_loop_in_thread(self.threadLoop_);
            if (lockNeeded) {
                pw_thread_loop_lock(self.threadLoop_);
            }

            const auto iterator = self.globalObjects_.find(id);
            if (iterator != self.globalObjects_.end()) {
                GlobalObject& object = iterator->second;
                if (object.proxy != nullptr) {
                    spa_hook_remove(&object.listener);
                    pw_proxy_destroy(object.proxy);
                }
                self.globalObjects_.erase(iterator);

                try {
                    if (self.globalRemoved) {
                        self.globalRemoved(id);
                    }
                } catch (...) {
                    // Exceptions must not cross the PipeWire C callback boundary.
                }
            }

            if (lockNeeded) {
                pw_thread_loop_unlock(self.threadLoop_);
            }
        },
    };

    if (pw_registry_add_listener(registry_, &registryListener_, &registryEvents, this) < 0) {
        stop();
        return false;
    }
    registryListenerInstalled_ = true;

    initialSyncSequence_ = pw_core_sync(core_, PW_ID_CORE, 0);
    if (initialSyncSequence_ < 0) {
        stop();
        return false;
    }

    running_.store(true, std::memory_order_release);
    if (pw_thread_loop_start(threadLoop_) < 0) {
        running_.store(false, std::memory_order_release);
        stop();
        return false;
    }
    loopStarted_ = true;
    return true;
}

void PipeWireContext::stop() {
    running_.store(false, std::memory_order_release);
    connectionAlive_.store(false, std::memory_order_release);

    if (threadLoop_ != nullptr && loopStarted_) {
        pw_thread_loop_stop(threadLoop_);
        loopStarted_ = false;
    }

    if (threadLoop_ != nullptr) {
        pw_thread_loop_lock(threadLoop_);
    }

    if (registryListenerInstalled_) {
        spa_hook_remove(&registryListener_);
        registryListenerInstalled_ = false;
    }

    for (auto& [id, object] : globalObjects_) {
        static_cast<void>(id);
        if (object.proxy != nullptr) {
            spa_hook_remove(&object.listener);
            pw_proxy_destroy(object.proxy);
            object.proxy = nullptr;
        }
    }
    globalObjects_.clear();

    if (registry_ != nullptr) {
        pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry_));
        registry_ = nullptr;
    }

    if (coreListenerInstalled_) {
        spa_hook_remove(&coreListener_);
        coreListenerInstalled_ = false;
    }

    if (core_ != nullptr) {
        pw_core_disconnect(core_);
        core_ = nullptr;
    }

    if (context_ != nullptr) {
        pw_context_destroy(context_);
        context_ = nullptr;
    }

    if (threadLoop_ != nullptr) {
        pw_thread_loop_unlock(threadLoop_);
        pw_thread_loop_destroy(threadLoop_);
        threadLoop_ = nullptr;
    }

    initialSyncSequence_ = -1;
    if (pipeWireInitialized_) {
        pw_deinit();
        pipeWireInitialized_ = false;
    }
}

bool PipeWireContext::isRunning() const noexcept {
    return running_.load(std::memory_order_acquire);
}
