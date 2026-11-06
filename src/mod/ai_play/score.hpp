#pragma once

// AI PLAY score: a lightweight progress/consistency counter, separate from model skill.
// Progress is awarded once per newly reached whole percentage point in an AI attempt.
// A death applies one fixed penalty. This is motivation/telemetry, not training evidence.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace aiplay::score {

inline constexpr std::int64_t kPointsPerPercent = 1;
inline constexpr std::int64_t kDeathPenalty = 10;

struct ScoreBoard {
    std::int64_t points = 0;
    double bestPercentThisAttempt = 0.0;
    bool attemptActive = false;
    bool deathApplied = false;

    void beginAttempt() {
        bestPercentThisAttempt = 0.0;
        attemptActive = true;
        deathApplied = false;
    }

    // Returns the number of points awarded by this observation.
    std::int64_t observeProgress(double percent) {
        if (!attemptActive || !std::isfinite(percent)) return 0;
        const double bounded = std::clamp(percent, 0.0, 100.0);
        const int before = static_cast<int>(std::floor(bestPercentThisAttempt));
        bestPercentThisAttempt = std::max(bestPercentThisAttempt, bounded);
        const int after = static_cast<int>(std::floor(bestPercentThisAttempt));
        const std::int64_t earned = static_cast<std::int64_t>(after - before) * kPointsPerPercent;
        if (earned > 0 && points <= std::numeric_limits<std::int64_t>::max() - earned)
            points += earned;
        else if (earned > 0)
            points = std::numeric_limits<std::int64_t>::max();
        return earned;
    }

    // A genuine death costs points once, even if multiple end hooks are observed.
    std::int64_t recordDeath() {
        if (!attemptActive || deathApplied) return 0;
        deathApplied = true;
        const std::int64_t lost = std::min(points, kDeathPenalty);
        points -= lost;
        attemptActive = false;
        return lost;
    }

    void finishClear() {
        if (!attemptActive) return;
        observeProgress(100.0);
        attemptActive = false;
        deathApplied = false;
    }

    void cancelAttempt() {
        attemptActive = false;
        bestPercentThisAttempt = 0.0;
        deathApplied = false;
    }
};

} // namespace aiplay::score
