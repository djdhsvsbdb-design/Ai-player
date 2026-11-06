#include "mod/ai_play/controller.hpp"

#include <cassert>

int main() {
    using namespace aiplay;
    using namespace aiplay::brain;
    using namespace aiplay::controller;

    Model model;
    for (int i = 0; i < 12; ++i) {
        TrainingExample e;
        e.mechanic = Mechanic::Wave;
        e.targetHeld = i < 6;
        e.verticalVelocity = 1.0;
        e.hazardGap = 20.0;
        model.examples.push_back(e);
    }
    assert(!readyFor(model, Mechanic::Wave));
    for (int i = 0; i < 12; ++i) {
        TrainingExample e;
        e.mechanic = Mechanic::Wave;
        e.targetHeld = i < 6;
        e.verticalVelocity = 1.0;
        e.hazardGap = 20.0;
        model.examples.push_back(e);
    }
    assert(readyFor(model, Mechanic::Wave));

    LiveObservation observation;
    observation.mechanic = Mechanic::Wave;
    observation.verticalVelocity = 1.0;
    observation.vehicleSize = 1.0;
    observation.nearestHazardGap = 20.0;
    observation.synchronized = true;
    auto cmd = decide(model, observation, true, false);
    assert(cmd.valid && !cmd.down); // unsafe session always releases
    observation.dead = true;
    cmd = decide(model, observation, true, true);
    assert(cmd.valid && !cmd.down); // death always releases
    observation.dead = false;
    observation.mechanic = Mechanic::Ship;
    cmd = decide(model, observation, true, true);
    assert(cmd.valid && !cmd.down); // untrained mechanic releases
    cmd = decide(model, observation, false, true);
    assert(!cmd.valid); // no spurious input
    return 0;
}
