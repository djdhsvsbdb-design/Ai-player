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

inline bool readyFor(const brain::Model& model, Mechanic mechanic) {
    std::size_t press = 0, release = 0;
    for (const auto& example : model.examples) {
        if (example.mechanic != mechanic) continue;
        if (example.targetHeld) ++press;
        else ++release;
    }
    // Avoid treating a tiny or one-sided sample as a trained policy.
    return press >= 8 && release >= 8 && press + release >= 24;
}

inline Command decide(const brain::Model& model,
                      const LiveObservation& observation,
                      bool currentlyHeld,
                      bool sessionSafe) {
    if (!sessionSafe || !observation.synchronized || observation.dead ||
        !isPlayableMechanic(observation.mechanic) ||
        !readyFor(model, observation.mechanic)) {
        return currentlyHeld ? Command{true, false} : Command{};
    }

    const auto prediction = brain::predict(model, observation);
    if (prediction.decision == brain::Decision::Abstain)
        return currentlyHeld ? Command{true, false} : Command{};
    const bool target = prediction.decision == brain::Decision::Press;
    return target == currentlyHeld ? Command{} : Command{true, target};
}

} // namespace aiplay::controller
