#include "mod/ai_play/learning.hpp"

#include <cassert>

int main() {
    using namespace aiplay;

    GlobalSkillMemory starter;
    applyStartingProfile(starter, StartingProfile::FirstInsaneDemon);
    assert(starter.mechanics.size() == 8);
    const auto& starterWave = starter.mechanics.at(Mechanic::Wave);
    assert(starterWave.overall.value == 0.60);
    assert(starterWave.overall.verifiedSamples == 0);
    assert(starterWave.overall.confidence == 0.0);
    // The preset is only a prior; real verified observations can improve or recalibrate it.
    VerifiedEvidence starterEvidence;
    starterEvidence.mechanic = Mechanic::Wave;
    starterEvidence.verified = true;
    starterEvidence.synchronized = true;
    starterEvidence.predictionError = 0.20;
    starterEvidence.timingError = 0.20;
    starterEvidence.controlError = 0.25;
    starterEvidence.recognitionError = 0.20;
    starterEvidence.adaptationError = 0.35;
    starterEvidence.consistencyError = 0.25;
    assert(applyVerifiedEvidence(starter, starterEvidence));
    assert(starter.mechanics.at(Mechanic::Wave).overall.verifiedSamples == 1);

    GlobalSkillMemory global;
    VerifiedEvidence noisy;
    noisy.mechanic = Mechanic::Wave;
    noisy.verified = true;
    noisy.synchronized = false;
    noisy.predictionError = 0.0;
    assert(!applyVerifiedEvidence(global, noisy));
    assert(global.mechanics.empty());

    VerifiedEvidence good;
    good.mechanic = Mechanic::Wave;
    good.verified = true;
    good.synchronized = true;
    good.predictionError = 0.1;
    good.timingError = 0.1;
    good.controlError = 0.1;
    good.recognitionError = 0.2;
    good.adaptationError = 0.3;
    good.consistencyError = 0.1;
    good.patternKey = "wave:steady-corridor";
    assert(applyVerifiedEvidence(global, good));

    const auto& wave = global.mechanics.at(Mechanic::Wave);
    assert(wave.overall.verifiedSamples == 1);
    assert(wave.overall.value > 0.8);
    assert(global.transferablePatterns.at("wave:steady-corridor").observations == 1);

    // Global mechanic skill is shared; level-specific observations remain separate.
    LevelExperienceMemory levelA;
    levelA.levelKey = "level-A";
    recordLevelAttempt(levelA);
    LevelObservation local;
    local.tick = 120;
    local.mechanic = Mechanic::Wave;
    local.localPatternKey = "section-3";
    local.progress = 0.4;
    local.verified = true;
    recordLevelObservation(levelA, local);
    assert(levelA.attempts == 1);
    assert(levelA.observations.size() == 1);
    assert(levelA.bestProgress == 0.4);

    LevelExperienceMemory levelB;
    levelB.levelKey = "level-B";
    assert(levelB.observations.empty());
    assert(global.mechanics.at(Mechanic::Wave).overall.verifiedSamples == 1);

    // Unverified progress cannot improve local best progress.
    local.progress = 1.0;
    local.verified = false;
    recordLevelObservation(levelB, local);
    assert(levelB.observations.empty());
    assert(levelB.bestProgress == 0.0);
    assert(levelB.attempts == 0);
    return 0;
}
