#include "AutoRecordController.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr int kStartDebounceCalls = 2;
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

void AutoRecordController::feedLevel(const float peakLinear) {
    const float peakDbfs = peakLinear > 0.0F && !std::isnan(peakLinear)
                               ? 20.0F * std::log10(peakLinear)
                               : -std::numeric_limits<float>::infinity();
    const bool aboveStartThreshold = peakDbfs > startThresholdDbfs_;

    const auto transitionTo = [this](const State nextState) {
        if (state_ == nextState) {
            return;
        }
        state_ = nextState;
        emit stateChanged(state_);
    };

    switch (state_) {
    case Idle:
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

        transitionTo(Recording);
        emit startRequested();
        emit preRollRequested(preRollDurationMs_);
        return;

    case Armed:
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
        transitionTo(Recording);
        emit startRequested();
        emit preRollRequested(preRollDurationMs_);
        return;

    case Recording:
        if (peakDbfs < stopThresholdDbfs_) {
            transitionTo(PostRoll);
            postRollTimer_.start();
        }
        return;

    case PostRoll:
        if (aboveStartThreshold) {
            postRollTimer_.invalidate();
            transitionTo(Recording);
            return;
        }

        if (!postRollTimer_.isValid()) {
            postRollTimer_.start();
        }
        if (postRollTimer_.elapsed() < postRollDurationMs_) {
            return;
        }

        postRollTimer_.invalidate();
        transitionTo(Idle);
        emit stopRequested();
        return;
    }
}
