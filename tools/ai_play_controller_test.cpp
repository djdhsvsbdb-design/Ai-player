#include "mod/ai_play/controller.hpp"

#include <cassert>

int main() {
    using namespace aiplay;
    using namespace aiplay::brain;
    using namespace aiplay::controller;

    Model model;
    for (int i = 0; i < 24; ++i) {
        TrainingExample e;
        e.mechanic = Mechanic::Wave;
        e.targetHeld = i < 12;
        e.verticalVelocity = 1.0;
        e.hazardGap = 20.0;
        model.examples.push_back(e);
        ++model.actionCounts[static_cast<std::size_t>(Mechanic::Wave)]
                            [e.targetHeld ? 1u : 0u];
    }
    assert(readyFor(model, Mechanic::Wave));
    assert(!readyFor(model, Mechanic::Ship));

    LiveObservation observation;
    observation.mechanic = Mechanic::Wave;
    observation.verticalVelocity = 1.0;
    observation.vehicleSize = 1.0;
    observation.nearestHazardGap = 20.0;
    observation.synchronized = true;

    auto result = decideDetailed(model, observation, true, false);
    assert(result.command.valid && !result.command.down);
    assert(result.reason == Reason::UnsafeSession);

    observation.dead = true;
    result = decideDetailed(model, observation, true, true);
    assert(result.command.valid && !result.command.down);
    assert(result.reason == Reason::PlayerDead);

    observation.dead = false;
    observation.mechanic = Mechanic::Ship;
    result = decideDetailed(model, observation, true, true);
    assert(result.command.valid && !result.command.down);
    assert(result.reason == Reason::InsufficientTraining);
    result = decideDetailed(model, observation, false, true);
    assert(!result.command.valid);

    observation.mechanic = Mechanic::Wave;
    observation.synchronized = false;
    result = decideDetailed(model, observation, false, true);
    assert(!result.command.valid);
    assert(result.reason == Reason::UnsynchronizedObservation);

    // The compatibility API preserves the original simple command contract.
    observation.synchronized = true;
    observation.dead = true;
    const auto command = decide(model, observation, true, true);
    assert(command.valid && !command.down);
    return 0;
}
