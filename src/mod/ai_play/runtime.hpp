#pragma once

// AI PLAY Phase B1: persistent starter profile and verified human-play progress.
// This runtime deliberately does not infer mechanic skill from a level percentage.
// The read-only observation adapter is telemetry; policy-grade verified evidence is
// still required before any mechanic skill can change.

#include "mod/ai_play/learning.hpp"
#include "mod/ai_play/brain.hpp"
#include "mod/ai_play/controller.hpp"
#include "mod/ai_play/score.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>

namespace aiplay::runtime {

inline GlobalSkillMemory g_globalSkill;
inline std::unordered_map<std::string, LevelExperienceMemory> g_levels;
inline std::string g_currentLevelKey;
inline bool g_loaded = false;
inline bool g_liveObservationEnabled = false;
inline std::filesystem::path g_memoryPath;
// Bounded, in-session state samples from the live game adapter. These are telemetry,
// not verified learning evidence, and are deliberately not promoted into global skill.
inline std::vector<LiveObservation> g_liveObservations;
inline std::vector<HumanActionSample> g_humanActions;
inline brain::Model g_brainModel;
inline bool g_aiButtonHeld = false;
inline bool g_aiWasDriving = false;
inline std::vector<HumanActionSample> g_aiActions;
inline std::uint64_t g_aiAttempts = 0;
inline std::uint64_t g_aiClears = 0;
inline score::ScoreBoard g_aiScore;

inline void save() {
    if (!g_loaded || g_memoryPath.empty()) return;
    const auto temp = std::filesystem::path(g_memoryPath.string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::out | std::ios::trunc);
        if (!out) return;
        out << "AIPLAY_MEMORY " << kLearningSchemaVersion << '\n';
        out << "P " << g_aiScore.points << '\n';
        out << std::setprecision(17);
        for (const auto& [mechanic, skill] : g_globalSkill.mechanics) {
            out << "S " << static_cast<unsigned>(mechanic);
            auto writeEstimate = [&](const SkillEstimate& e) {
                out << ' ' << e.value << ' ' << e.confidence << ' '
                    << e.meanError << ' ' << e.verifiedSamples;
            };
            writeEstimate(skill.overall);
            for (const auto& dimension : skill.dimensions) writeEstimate(dimension);
            out << '\n';
        }
        for (const auto& [key, level] : g_levels)
            out << "L " << key << ' ' << level.attempts << ' '
                << level.bestProgress << '\n';
        out.flush();
        if (!out) return;
    }

    std::error_code ec;
    std::filesystem::rename(temp, g_memoryPath, ec);
    if (ec) {
        // Some filesystems do not replace an existing destination on rename.
        std::filesystem::remove(g_memoryPath, ec);
        ec.clear();
        std::filesystem::rename(temp, g_memoryPath, ec);
    }
    if (ec) std::filesystem::remove(temp, ec);
}

inline void load(const std::filesystem::path& path) {
    if (g_loaded) return;
    g_loaded = true;
    g_memoryPath = path;
    applyStartingProfile(g_globalSkill, StartingProfile::FirstInsaneDemon);
    const auto trainingPath = g_memoryPath.parent_path() / "ai_play_training_v1.dat";
    brain::trainFromFile(g_brainModel, trainingPath.string());

    std::ifstream in(g_memoryPath);
    std::string magic;
    std::uint32_t version = 0;
    if (!(in >> magic >> version) || magic != "AIPLAY_MEMORY" ||
        version != kLearningSchemaVersion) {
        save();
        return;
    }

    char kind = 0;
    while (in >> kind) {
        if (kind == 'S') {
            unsigned rawMechanic = 0;
            if (!(in >> rawMechanic) || rawMechanic >= static_cast<unsigned>(Mechanic::Unknown))
                break;
            auto& skill = g_globalSkill.mechanics[static_cast<Mechanic>(rawMechanic)];
            auto readEstimate = [&](SkillEstimate& e) {
                return static_cast<bool>(in >> e.value >> e.confidence
                                           >> e.meanError >> e.verifiedSamples);
            };
            if (!readEstimate(skill.overall)) break;
            bool ok = true;
            for (auto& dimension : skill.dimensions)
                if (!readEstimate(dimension)) { ok = false; break; }
            if (!ok) break;
            skill.overall.value = clamp01(skill.overall.value);
            skill.overall.confidence = clamp01(skill.overall.confidence);
            for (auto& dimension : skill.dimensions) {
                dimension.value = clamp01(dimension.value);
                dimension.confidence = clamp01(dimension.confidence);
            }
        } else if (kind == 'P') {
            std::int64_t points = 0;
            if (!(in >> points)) break;
            g_aiScore.points = std::max<std::int64_t>(0, points);
        } else if (kind == 'L') {
            std::string key;
            LevelExperienceMemory level;
            if (!(in >> key >> level.attempts >> level.bestProgress)) break;
            level.levelKey = key;
            level.bestProgress = clamp01(level.bestProgress);
            g_levels[key] = std::move(level);
        } else {
            break;
        }
    }
}

inline void setLiveObservationEnabled(bool enabled) {
    g_liveObservationEnabled = enabled;
}

inline void beginLevel(int levelId) {
    g_currentLevelKey = std::to_string(levelId);
    g_liveObservations.clear();
    g_humanActions.clear();
    auto& level = g_levels[g_currentLevelKey];
    level.levelKey = g_currentLevelKey;
}

inline bool shouldSampleLiveObservation(std::uint64_t tick, Mechanic mechanic) {
    if (!g_loaded || !g_liveObservationEnabled || g_currentLevelKey.empty() ||
        !isPlayableMechanic(mechanic)) return false;
    if (g_liveObservations.empty()) return true;
    const auto& last = g_liveObservations.back();
    return last.mechanic != mechanic || tick < last.tick || tick - last.tick >= 24;
}

inline void recordLiveObservation(const LiveObservation& observation) {
    if (!g_loaded || !g_liveObservationEnabled || g_currentLevelKey.empty() || !observation.synchronized ||
        observation.dead || !isPlayableMechanic(observation.mechanic)) return;

    // The adapter samples at 10 Hz, plus immediately on mode transitions. Keep
    // the buffer bounded so long mobile sessions cannot grow memory indefinitely.
    if (!g_liveObservations.empty()) {
        const auto& last = g_liveObservations.back();
        if (last.mechanic == observation.mechanic &&
            observation.tick >= last.tick && observation.tick - last.tick < 24)
            return;
    }
    constexpr std::size_t kMaxLiveObservations = 4096;
    if (g_liveObservations.size() >= kMaxLiveObservations)
        g_liveObservations.erase(g_liveObservations.begin());
    g_liveObservations.push_back(observation);
}

inline void recordHumanAction(const HumanActionSample& action) {
    if (!g_loaded || !g_liveObservationEnabled || g_currentLevelKey.empty() ||
        !isPlayableMechanic(action.mechanic)) return;
    constexpr std::size_t kMaxHumanActions = 8192;
    if (g_humanActions.size() >= kMaxHumanActions)
        g_humanActions.erase(g_humanActions.begin());
    g_humanActions.push_back(action);
}

// Persist bounded human demonstrations automatically at each attempt boundary.
// These are imitation-learning examples, not verified mechanic-skill updates.
inline void persistDemonstrations(double progress, bool completed) {
    if (g_memoryPath.empty() || g_humanActions.empty()) return;
    const auto path = g_memoryPath.parent_path() / "ai_play_training_v1.dat";
    constexpr std::uintmax_t kMaxTrainingBytes = 16u * 1024u * 1024u;
    std::error_code ec;
    auto size = std::filesystem::exists(path, ec) ? std::filesystem::file_size(path, ec) : 0u;
    if (ec) return;
    // Keep storage bounded. Once the cap is reached, start a fresh dataset with
    // the current attempt rather than growing indefinitely on a mobile device.
    const bool reset = size > kMaxTrainingBytes;
    const bool needsHeader = reset || size == 0;
    std::ofstream out(path, std::ios::out | (needsHeader ? std::ios::trunc : std::ios::app));
    if (!out) return;
    if (needsHeader) out << "AIPLAY_DEMOS 1\n";
    out << std::setprecision(9);
    for (const auto& a : g_humanActions) {
        out << "A " << g_currentLevelKey << ' ' << (completed ? 1 : 0) << ' '
            << clamp01(progress) << ' ' << a.tick << ' '
            << static_cast<unsigned>(a.mechanic) << ' '
            << a.x << ' ' << a.y << ' ' << a.verticalVelocity << ' '
            << a.vehicleSize << ' ' << a.nearestHazardGap << ' '
            << a.nearestHazardVerticalOffset << ' ' << a.nearestHazardType << ' '
            << a.nearbyHazardCount << ' ' << (a.upsideDown ? 1 : 0) << ' '
            << (a.buttonHeldBefore ? 1 : 0) << ' ' << (a.down ? 1 : 0) << '\n';
    }
    out.flush();
}

// AI-generated trajectories stay separate from human demonstrations. Failed
// rollouts are discarded rather than being trained as if their actions were correct.
inline void recordAIAction(const HumanActionSample& action) {
    if (!g_loaded || !g_liveObservationEnabled || !g_aiWasDriving ||
        !isPlayableMechanic(action.mechanic)) return;
    constexpr std::size_t kMaxAIActions = 8192;
    if (g_aiActions.size() >= kMaxAIActions) g_aiActions.erase(g_aiActions.begin());
    g_aiActions.push_back(action);
}

inline bool hasAIAttempt() {
    return g_aiWasDriving || !g_aiActions.empty() || g_aiScore.attemptActive;
}

// Start scoring only after the policy is actually armed. Percent is Geometry Dash's
// own level progress; duplicate frame observations never award duplicate points.
inline void observeAIProgress(double percent) {
    if (!g_aiWasDriving) return;
    if (!g_aiScore.attemptActive) g_aiScore.beginAttempt();
    g_aiScore.observeProgress(percent);
}

inline void finishAIAttempt(bool completed) {
    if (!g_aiWasDriving && g_aiActions.empty() && !g_aiScore.attemptActive) return;
    if (completed) g_aiScore.finishClear();
    else g_aiScore.recordDeath();
    ++g_aiAttempts;
    if (completed && !g_aiActions.empty()) {
        // Only a genuine in-game clear promotes an AI trajectory. Death trajectories
        // are not self-labelled, which avoids reinforcing the policy's own mistakes.
        auto previous = std::move(g_humanActions);
        g_humanActions = std::move(g_aiActions);
        g_aiActions.clear();
        persistDemonstrations(1.0, true);
        g_humanActions = std::move(previous);
        ++g_aiClears;
        const auto trainingPath = g_memoryPath.parent_path() / "ai_play_training_v1.dat";
        brain::trainFromFile(g_brainModel, trainingPath.string());
    } else {
        g_aiActions.clear();
    }
    g_aiWasDriving = false;
    g_aiButtonHeld = false;
    save();
}

inline brain::Prediction predict(const LiveObservation& observation) {
    return brain::predict(g_brainModel, observation);
}

inline void recordHumanAttempt(double progress, bool completed) {
    if (!g_loaded || !g_liveObservationEnabled || g_currentLevelKey.empty()) return;
    auto found = g_levels.find(g_currentLevelKey);
    if (found == g_levels.end()) return;

    auto& level = found->second;
    recordLevelAttempt(level);
    LevelObservation observation;
    observation.mechanic = Mechanic::Unknown;
    observation.localPatternKey = completed ? "human-clear" : "human-death";
    observation.progress = completed ? 1.0 : clamp01(progress);
    observation.error = completed ? 0.0 : 1.0 - observation.progress;
    observation.verified = true;
    recordLevelObservation(level, observation);
    persistDemonstrations(progress, completed);
    // Rebuild the compact policy after every attempt so new demonstrations can
    // influence the next run without requiring a mod/game restart.
    const auto trainingPath = g_memoryPath.parent_path() / "ai_play_training_v1.dat";
    brain::trainFromFile(g_brainModel, trainingPath.string());
    g_humanActions.clear();
    g_liveObservations.clear();
    save();
}

} // namespace aiplay::runtime
