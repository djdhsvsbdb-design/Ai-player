#pragma once

// Offline AI PLAY controller. The controller only emits real button-state edges.
// It refuses to drive a mechanic until its local demonstration set has enough
// balanced examples; abstention always releases the button.

#include "mod/ai_play/brain.hpp"

#include <cstddef>

namespace aiplay::controller {

struct Command {
    bool valid = false;
    bool down = false;
};

enum class Reason : std::uint8_t {
    ActionIssued,
    NoChangeNeeded,
    UnsafeSession,
    UnsynchronizedObservation,
    PlayerDead,
    UnsupportedMechanic,
    InsufficientTraining,
    ModelAbstained
};

struct DecisionResult {
    Command command;
    Reason reason = Reason::NoChangeNeeded;
    double confidence = 0.0;
    double nearestDistance = 0.0;
    std::size_t neighbors = 0;
};

inline bool readyFor(const brain::Model& model, Mechanic mechanic) {
    if (!isPlayableMechanic(mechanic)) return false;
    const auto index = static_cast<std::size_t>(mechanic);
    const auto releases = model.actionCounts[index][0];
    const auto presses = model.actionCounts[index][1];
    // Avoid treating a tiny or one-sided sample as a trained policy.
    return presses >= 8 && releases >= 8 && presses + releases >= 24;
}

inline DecisionResult decideDetailed(const brain::Model& model,
                                     const LiveObservation& observation,
                                     bool currentlyHeld,
                                     bool sessionSafe) {
    auto blocked = [currentlyHeld](Reason reason) {
        return DecisionResult{
            currentlyHeld ? Command{true, false} : Command{}, reason, 0.0, 0.0, 0
        };
    };

    if (!sessionSafe) return blocked(Reason::UnsafeSession);
    if (!observation.synchronized)
        return blocked(Reason::UnsynchronizedObservation);
    if (observation.dead) return blocked(Reason::PlayerDead);
    if (!isPlayableMechanic(observation.mechanic))
        return blocked(Reason::UnsupportedMechanic);
    if (!readyFor(model, observation.mechanic))
        return blocked(Reason::InsufficientTraining);

    const auto prediction = brain::predict(model, observation);
    if (prediction.decision == brain::Decision::Abstain) {
        auto result = blocked(Reason::ModelAbstained);
        result.confidence = prediction.confidence;
        result.nearestDistance = prediction.distance;
        result.neighbors = prediction.neighbors;
        return result;
    }

    const bool target = prediction.decision == brain::Decision::Press;
    DecisionResult result;
    result.reason = target == currentlyHeld
        ? Reason::NoChangeNeeded : Reason::ActionIssued;
    result.command = target == currentlyHeld ? Command{} : Command{true, target};
    result.confidence = prediction.confidence;
    result.nearestDistance = prediction.distance;
    result.neighbors = prediction.neighbors;
    return result;
}

inline Command decide(const brain::Model& model,
                      const LiveObservation& observation,
                      bool currentlyHeld,
                      bool sessionSafe) {
    return decideDetailed(model, observation, currentlyHeld, sessionSafe).command;
}

} // namespace aiplay::controller
