#pragma once

// Bounded, allocation-free AI PLAY counters. Intended for the game thread only.
// This records policy behavior; it does not claim that an issued input was successful.

#include "mod/ai_play/controller.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace aiplay::diagnostics {

struct MechanicCounters {
    std::uint64_t decisions = 0;
    std::uint64_t issuedPresses = 0;
    std::uint64_t issuedReleases = 0;
    std::uint64_t unchanged = 0;
    std::uint64_t abstained = 0;
    std::uint64_t safetyBlocks = 0;
    std::uint64_t unsupported = 0;
    std::uint64_t predictionChecks = 0;
    std::uint64_t predictionMatches = 0;
    double confidenceSum = 0.0;
    double maxObservedDistance = 0.0;

    double actionCoverage() const {
        return decisions == 0 ? 0.0
            : static_cast<double>(decisions - abstained - safetyBlocks - unsupported) /
              static_cast<double>(decisions);
    }

    double predictionAgreement() const {
        return predictionChecks == 0 ? 0.0
            : static_cast<double>(predictionMatches) /
              static_cast<double>(predictionChecks);
    }

    double meanConfidence() const {
        return decisions == 0 ? 0.0
            : confidenceSum / static_cast<double>(decisions);
    }
};

struct Snapshot {
    std::array<MechanicCounters, 8> mechanics{};
    std::uint64_t invalidMechanicEvents = 0;
};

inline void increment(std::uint64_t& value) {
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

inline bool recordDecision(Snapshot& snapshot, Mechanic mechanic,
                           const controller::DecisionResult& result) {
    if (!isPlayableMechanic(mechanic)) {
        increment(snapshot.invalidMechanicEvents);
        return false;
    }

    auto& counters = snapshot.mechanics[static_cast<std::size_t>(mechanic)];
    increment(counters.decisions);
    counters.confidenceSum += clamp01(result.confidence);
    if (result.neighbors > 0)
        counters.maxObservedDistance =
            std::max(counters.maxObservedDistance, result.nearestDistance);

    switch (result.reason) {
        case controller::Reason::ActionIssued:
            if (result.command.down) increment(counters.issuedPresses);
            else increment(counters.issuedReleases);
            break;
        case controller::Reason::NoChangeNeeded:
            increment(counters.unchanged);
            break;
        case controller::Reason::ModelAbstained:
            increment(counters.abstained);
            break;
        case controller::Reason::UnsafeSession:
        case controller::Reason::UnsynchronizedObservation:
        case controller::Reason::PlayerDead:
        case controller::Reason::InsufficientTraining:
            increment(counters.safetyBlocks);
            break;
        case controller::Reason::UnsupportedMechanic:
            increment(counters.unsupported);
            break;
    }
    return true;
}

// Compare a model's target with a separately verified reference action.
// Callers must only submit synchronized, genuinely observed labels.
inline bool recordVerifiedPrediction(Snapshot& snapshot, Mechanic mechanic,
                                     bool predictedHeld, bool observedHeld,
                                     bool verified, bool synchronized) {
    if (!verified || !synchronized || !isPlayableMechanic(mechanic))
        return false;
    auto& counters = snapshot.mechanics[static_cast<std::size_t>(mechanic)];
    increment(counters.predictionChecks);
    if (predictedHeld == observedHeld)
        increment(counters.predictionMatches);
    return true;
}

inline void reset(Snapshot& snapshot) {
    snapshot = Snapshot{};
}

} // namespace aiplay::diagnostics
