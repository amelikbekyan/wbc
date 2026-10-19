// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <deque>

#include "core/IClock.h"

namespace wbc {

// Counts frames over a sliding window rather than since start, so that the
// displayed rate reacts when the pipeline slows down instead of averaging the
// problem away.
class PipelineStats {
public:
    struct Snapshot {
        double fps = 0.0;
        double latencyMs = 0.0;
        std::uint64_t delivered = 0;
        std::uint64_t dropped = 0;
    };

    explicit PipelineStats(const IClock& clock, double windowSeconds = 1.0);

    void RecordDelivered(double latencyMs);
    void RecordDropped();
    Snapshot Current() const;
    void Reset();

private:
    struct Entry {
        std::chrono::steady_clock::time_point at;
        double latencyMs;
    };

    void Evict() const;

    const IClock& clock_;
    double windowSeconds_;
    mutable std::deque<Entry> entries_;
    std::uint64_t delivered_ = 0;
    std::uint64_t dropped_ = 0;
};

}  // namespace wbc
