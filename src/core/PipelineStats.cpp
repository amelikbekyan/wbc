// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/PipelineStats.h"

namespace wbc {

PipelineStats::PipelineStats(const IClock& clock, double windowSeconds)
    : clock_(clock), windowSeconds_(windowSeconds) {}

void PipelineStats::RecordDelivered(double latencyMs) {
    entries_.push_back({clock_.Now(), latencyMs});
    ++delivered_;
    Evict();
}

void PipelineStats::RecordDropped() {
    ++dropped_;
}

void PipelineStats::Evict() const {
    const auto cutoff = clock_.Now() - std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                           std::chrono::duration<double>(windowSeconds_));
    while (!entries_.empty() && entries_.front().at < cutoff) {
        entries_.pop_front();
    }
}

PipelineStats::Snapshot PipelineStats::Current() const {
    Evict();

    Snapshot snapshot;
    snapshot.delivered = delivered_;
    snapshot.dropped = dropped_;

    if (entries_.empty()) {
        return snapshot;
    }

    double latencySum = 0.0;
    for (const auto& entry : entries_) {
        latencySum += entry.latencyMs;
    }
    snapshot.latencyMs = latencySum / static_cast<double>(entries_.size());

    // When there is no interval to measure, count events instead — N frames
    // inside the window is a rate of N per window.
    const auto span = std::chrono::duration<double>(entries_.back().at - entries_.front().at).count();
    if (entries_.size() == 1 || span <= 0.0) {
        snapshot.fps = static_cast<double>(entries_.size()) / windowSeconds_;
    } else {
        snapshot.fps = static_cast<double>(entries_.size() - 1) / span;
    }
    return snapshot;
}

void PipelineStats::Reset() {
    entries_.clear();
    delivered_ = 0;
    dropped_ = 0;
}

}  // namespace wbc
