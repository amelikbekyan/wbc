// SPDX-FileCopyrightText: 2026 Arthur Melikbekyan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "camera/FormatNegotiator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <tuple>

namespace wbc {
namespace {

// Lower is better. The ranking is lexicographic in intent: a format that fits
// within the requested size beats one that overshoots, a preferred fourcc
// beats an unfamiliar one, and frame rate only breaks ties.
struct Score {
    int overshoot;
    std::size_t preference;
    long long areaDeficit;
    double fpsDistance;

    bool operator<(const Score& other) const {
        return std::tie(overshoot, preference, areaDeficit, fpsDistance) <
               std::tie(other.overshoot, other.preference, other.areaDeficit, other.fpsDistance);
    }
};

std::size_t PreferenceRank(std::uint32_t fourcc, const std::vector<std::uint32_t>& preference) {
    const auto found = std::find(preference.begin(), preference.end(), fourcc);
    if (found == preference.end()) {
        return preference.size();
    }
    return static_cast<std::size_t>(std::distance(preference.begin(), found));
}

Score ScoreOf(const FormatOption& option, const FormatRequest& request) {
    const bool fits = option.width <= request.width && option.height <= request.height;
    const long long requestedArea = static_cast<long long>(request.width) * request.height;
    const long long area = static_cast<long long>(option.width) * option.height;

    Score score{};
    score.overshoot = fits ? 0 : 1;
    score.preference = PreferenceRank(option.fourcc, request.fourccPreference);
    // Within each group, the closest area to what was asked for.
    score.areaDeficit = std::llabs(requestedArea - area);
    score.fpsDistance = std::abs(option.fps - request.fps);
    return score;
}

}  // namespace

std::optional<FormatOption> Negotiate(const std::vector<FormatOption>& supported,
                                      const FormatRequest& request) {
    if (supported.empty()) {
        return std::nullopt;
    }

    const FormatOption* best = nullptr;
    Score bestScore{std::numeric_limits<int>::max(), 0, 0, 0.0};
    for (const FormatOption& option : supported) {
        const Score score = ScoreOf(option, request);
        if (best == nullptr || score < bestScore) {
            best = &option;
            bestScore = score;
        }
    }
    return *best;
}

}  // namespace wbc
