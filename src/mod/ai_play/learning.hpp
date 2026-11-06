#pragma once

// AI PLAY Phase A: platform-independent learning records and verified updates.
// This header deliberately does not depend on Geode or Windows/Android hooks.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace aiplay {

enum class Mechanic : std::uint8_t {
    Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing, Unknown
};

enum class SkillDimension : std::uint8_t {
    Timing, Control, PatternRecognition, Adaptation, Consistency, Count
};

inline constexpr std::uint32_t kLearningSchemaVersion = 1;
inline constexpr std::size_t kSkillDimensionCount =
    static_cast<std::size_t>(SkillDimension::Count);

inline double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

// Skill is an estimate based on verified evidence, not a level or an attempt counter.
// confidence remains low until enough independent observations have been collected.
struct SkillEstimate {
    double value = 0.15;
    double confidence = 0.0;
    double meanError = 1.0;
    std::uint32_t verifiedSamples = 0;
};

struct MechanicSkill {
    SkillEstimate overall;
    SkillEstimate dimensions[kSkillDimensionCount]{};
};

// A compact, platform-neutral description of one verified decision.
// Errors are normalized to [0, 1]. Unverified or desynchronized observations are ignored.
struct VerifiedEvidence {
    Mechanic mechanic = Mechanic::Unknown;
    double predictionError = 1.0;
    double timingError = 1.0;
    double controlError = 1.0;
    double recognitionError = 1.0;
    double adaptationError = 1.0;
    double consistencyError = 1.0;
    bool verified = false;
    bool synchronized = false;
    std::string patternKey;
};

struct PatternExperience {
    std::uint32_t successes = 0;
    std::uint32_t observations = 0;
    double meanError = 1.0;
};

struct GlobalSkillMemory {
    std::uint32_t schemaVersion = kLearningSchemaVersion;
    std::unordered_map<Mechanic, MechanicSkill> mechanics;
    std::unordered_map<std::string, PatternExperience> transferablePatterns;
};

struct LevelObservation {
    std::uint64_t tick = 0;
    Mechanic mechanic = Mechanic::Unknown;
    std::string localPatternKey;
    double progress = 0.0;
    double error = 1.0;
    bool verified = false;
};

struct LevelExperienceMemory {
    std::uint32_t schemaVersion = kLearningSchemaVersion;
    std::string levelKey;
    std::vector<LevelObservation> observations;
    std::uint64_t attempts = 0;
    double bestProgress = 0.0;
};

inline void updateEstimate(SkillEstimate& estimate, double error,
                           std::uint32_t priorSamples) {
    error = clamp01(error);
    // Evidence has diminishing influence as the estimate becomes established.
    const double alpha = priorSamples < 4 ? 0.25 : 0.10;
    estimate.meanError = priorSamples == 0
        ? error
        : (1.0 - alpha) * estimate.meanError + alpha * error;
    estimate.value = clamp01(1.0 - estimate.meanError);
    ++estimate.verifiedSamples;
    estimate.confidence = clamp01(
        static_cast<double>(estimate.verifiedSamples) /
        (static_cast<double>(estimate.verifiedSamples) + 8.0));
}

// Returns false unless the sample is verified and synchronized. Attempts alone never
// change skill. Global updates are mechanic-specific; level coordinates stay in level memory.
inline bool applyVerifiedEvidence(GlobalSkillMemory& memory,
                                  const VerifiedEvidence& evidence) {
    if (!evidence.verified || !evidence.synchronized ||
        evidence.mechanic == Mechanic::Unknown) {
        return false;
    }

    auto& skill = memory.mechanics[evidence.mechanic];
    const auto prior = skill.overall.verifiedSamples;
    updateEstimate(skill.overall, evidence.predictionError, prior);

    const double errors[kSkillDimensionCount] = {
        clamp01(evidence.timingError),
        clamp01(evidence.controlError),
        clamp01(evidence.recognitionError),
        clamp01(evidence.adaptationError),
        clamp01(evidence.consistencyError)
    };
    for (std::size_t i = 0; i < kSkillDimensionCount; ++i) {
        updateEstimate(skill.dimensions[i], errors[i],
                       skill.dimensions[i].verifiedSamples);
    }

    if (!evidence.patternKey.empty()) {
        auto& pattern = memory.transferablePatterns[evidence.patternKey];
        const double error = clamp01(evidence.predictionError);
        pattern.meanError = pattern.observations == 0
            ? error
            : 0.9 * pattern.meanError + 0.1 * error;
        ++pattern.observations;
        if (error <= 0.25) ++pattern.successes;
    }
    return true;
}

inline void recordLevelAttempt(LevelExperienceMemory& memory) {
    ++memory.attempts;
}

inline void recordLevelObservation(LevelExperienceMemory& memory,
                                   const LevelObservation& observation) {
    if (observation.verified) {
        memory.bestProgress = std::max(
            memory.bestProgress, clamp01(observation.progress));
        memory.observations.push_back(observation);
    }
}

} // namespace aiplay
