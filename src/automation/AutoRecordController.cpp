#include "AutoRecordController.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
constexpr int kStartDebounceCalls = 2;

const char* stateName(const AutoRecordController::State state) noexcept {
    switch (state) {
    case AutoRecordController::Idle: return "idle";
    case AutoRecordController::Armed: return "armed_waiting";
    case AutoRecordController::Recording: return "recording";
    case AutoRecordController::PostRoll: return "post_roll";
    }
    return "unknown";
}
}

AutoRecordController::AutoRecordController(QObject* const parent)
    : QObject(parent) {}

AutoRecordController::State AutoRecordController::state() const noexcept {
    return state_;
}

float AutoRecordController::startThreshold() const noexcept {
    return startThresholdDbfs_;
}

void AutoRecordController::setStartThreshold(const float thresholdDbfs) noexcept {
    if (!std::isnan(thresholdDbfs)) {
        startThresholdDbfs_ = thresholdDbfs;
    }
}

float AutoRecordController::stopThreshold() const noexcept {
    return stopThresholdDbfs_;
}

void AutoRecordController::setStopThreshold(const float thresholdDbfs) noexcept {
    if (!std::isnan(thresholdDbfs)) {
        stopThresholdDbfs_ = thresholdDbfs;
    }
}

int AutoRecordController::holdTimeMs() const noexcept {
    return holdTimeMs_;
}

void AutoRecordController::setHoldTimeMs(const int durationMs) noexcept {
    holdTimeMs_ = std::max(0, durationMs);
}

int AutoRecordController::preRollDurationMs() const noexcept {
    return preRollDurationMs_;
}

void AutoRecordController::setPreRollDurationMs(const int durationMs) noexcept {
    preRollDurationMs_ = std::max(0, durationMs);
}

int AutoRecordController::postRollDurationMs() const noexcept {
    return postRollDurationMs_;
}

void AutoRecordController::setPostRollDurationMs(const int durationMs) noexcept {
    postRollDurationMs_ = std::max(0, durationMs);
}

int AutoRecordController::silenceDurationMs() const noexcept {
    return silenceDurationMs_;
}

void AutoRecordController::setSilenceDurationMs(const int durationMs) noexcept {
    silenceDurationMs_ = std::max(0, durationMs);
}

bool AutoRecordController::autoStartEnabled() const noexcept {
    return autoStartEnabled_;
}

void AutoRecordController::setAutoStartEnabled(const bool enabled) noexcept {
    autoStartEnabled_ = enabled;
}

bool AutoRecordController::autoStopEnabled() const noexcept {
    return autoStopEnabled_;
}

void AutoRecordController::setAutoStopEnabled(const bool enabled) noexcept {
    autoStopEnabled_ = enabled;
}

void AutoRecordController::reset() noexcept {
    state_ = Idle;
    consecutiveStartPeaks_ = 0;
    armedTimer_.invalidate();
    postRollTimer_.invalidate();
    silenceTimer_.invalidate();
}

void AutoRecordController::feedLevel(const float peakLinear) {
    const float peakDbfs = peakLinear > 0.0F && !std::isnan(peakLinear)
                               ? 20.0F * std::log10(peakLinear)
                               : -std::numeric_limits<float>::infinity();
    const bool aboveStartThreshold = peakDbfs >= startThresholdDbfs_;

    const auto transitionTo = [this](const State nextState) {
        if (state_ == nextState) {
            return;
        }
        std::fprintf(stderr, "[abracapture] automation: %s -> %s\n",
                     stateName(state_), stateName(nextState));
        state_ = nextState;
        emit stateChanged(state_);
    };

    switch (state_) {
    case Idle:
        if (!autoStartEnabled_) {
            return;
        }
        if (!aboveStartThreshold) {
            consecutiveStartPeaks_ = 0;
            return;
        }

        ++consecutiveStartPeaks_;
        if (consecutiveStartPeaks_ < kStartDebounceCalls) {
            return;
        }

        consecutiveStartPeaks_ = 0;
        transitionTo(Armed);
        armedTimer_.start();
        if (holdTimeMs_ > 0) {
            return;
        }

        std::fprintf(stderr,
                     "[abracapture] automation: start threshold crossed -> recording\n");
        transitionTo(Recording);
        emit startRequested();
        emit preRollRequested(preRollDurationMs_);
        return;

    case Armed:
        if (!autoStartEnabled_) {
            return;
        }
        if (!aboveStartThreshold) {
            armedTimer_.invalidate();
            transitionTo(Idle);
            return;
        }

        if (!armedTimer_.isValid()) {
            armedTimer_.start();
        }
        if (armedTimer_.elapsed() < holdTimeMs_) {
            return;
        }

        armedTimer_.invalidate();
        std::fprintf(stderr,
                     "[abracapture] automation: start threshold crossed -> recording\n");
        transitionTo(Recording);
        emit startRequested();
        emit preRollRequested(preRollDurationMs_);
        return;

    case Recording:
        if (!autoStopEnabled_) {
            return;
        }
        if (peakDbfs < stopThresholdDbfs_) {
            transitionTo(PostRoll);
            silenceTimer_.start();
            postRollTimer_.invalidate();
            std::fprintf(stderr, "[abracapture] automation: silence started\n");
        }
        return;

    case PostRoll:
        if (!autoStopEnabled_) {
            silenceTimer_.invalidate();
            postRollTimer_.invalidate();
            transitionTo(Recording);
            return;
        }
        if (peakDbfs >= stopThresholdDbfs_) {
            const qint64 elapsed = silenceTimer_.isValid() ? silenceTimer_.elapsed() : 0;
            std::fprintf(stderr, "[abracapture] automation: silence cancelled after %lld ms\n",
                         static_cast<long long>(elapsed));
            silenceTimer_.invalidate();
            postRollTimer_.invalidate();
            transitionTo(Recording);
            return;
        }

        if (!silenceTimer_.isValid()) {
            silenceTimer_.start();
        }
        if (silenceTimer_.elapsed() < silenceDurationMs_) {
            return;
        }

        if (!postRollTimer_.isValid()) {
            postRollTimer_.start();
            std::fprintf(stderr, "[abracapture] automation: silence duration reached %d ms\n",
                         silenceDurationMs_);
        }
        if (postRollTimer_.elapsed() < postRollDurationMs_) {
            return;
        }

        silenceTimer_.invalidate();
        postRollTimer_.invalidate();
        transitionTo(Idle);
        std::fprintf(stderr, "[abracapture] automation: auto-stop requested\n");
        emit automaticStopRequested();
        return;
    }
}
