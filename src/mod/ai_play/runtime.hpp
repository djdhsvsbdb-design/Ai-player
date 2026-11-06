#pragma once

// AI PLAY Phase B1: persistent starter profile and verified human-play progress.
// This runtime deliberately does not infer mechanic skill from a level percentage.
// The read-only observation adapter is telemetry; policy-grade verified evidence is
// still required before any mechanic skill can change.

#include "mod/ai_play/learning.hpp"

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

inline void save() {
    if (!g_loaded || g_memoryPath.empty()) return;
    const auto temp = std::filesystem::path(g_memoryPath.string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::out | std::ios::trunc);
        if (!out) return;
        out << "AIPLAY_MEMORY " << kLearningSchemaVersion << '\n';
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
    auto& level = g_levels[g_currentLevelKey];
    level.levelKey = g_currentLevelKey;
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
    save();
}

} // namespace aiplay::runtime
