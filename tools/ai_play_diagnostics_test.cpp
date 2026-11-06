#include "mod/ai_play/diagnostics.hpp"

#include <cassert>

int main() {
    using namespace aiplay;
    using namespace aiplay::controller;
    using namespace aiplay::diagnostics;

    Snapshot stats;
    DecisionResult issued;
    issued.command = {true, true};
    issued.reason = Reason::ActionIssued;
    issued.confidence = 0.9;
    issued.nearestDistance = 0.2;
    issued.neighbors = 5;
    assert(recordDecision(stats, Mechanic::Wave, issued));

    DecisionResult abstained;
    abstained.reason = Reason::ModelAbstained;
    assert(recordDecision(stats, Mechanic::Wave, abstained));

    DecisionResult blocked;
    blocked.reason = Reason::UnsafeSession;
    assert(recordDecision(stats, Mechanic::Wave, blocked));

    const auto& wave = stats.mechanics[static_cast<std::size_t>(Mechanic::Wave)];
    assert(wave.decisions == 3);
    assert(wave.issuedPresses == 1);
    assert(wave.abstained == 1);
    assert(wave.safetyBlocks == 1);
    assert(wave.actionCoverage() >= 0.0 && wave.actionCoverage() <= 1.0);
    assert(wave.meanConfidence() >= 0.0 && wave.meanConfidence() <= 1.0);
    assert(wave.maxObservedDistance == 0.2);

    assert(recordVerifiedPrediction(stats, Mechanic::Wave, true, true, true, true));
    assert(recordVerifiedPrediction(stats, Mechanic::Wave, true, false, true, true));
    assert(!recordVerifiedPrediction(stats, Mechanic::Wave, true, false, false, true));
    assert(!recordVerifiedPrediction(stats, Mechanic::Unknown, true, true, true, true));
    assert(wave.predictionChecks == 2);
    assert(wave.predictionMatches == 1);
    assert(wave.predictionAgreement() == 0.5);

    assert(!recordDecision(stats, Mechanic::Unknown, issued));
    assert(stats.invalidMechanicEvents == 1);
    reset(stats);
    assert(stats.invalidMechanicEvents == 0);
    assert(stats.mechanics[static_cast<std::size_t>(Mechanic::Wave)].decisions == 0);
    return 0;
}
