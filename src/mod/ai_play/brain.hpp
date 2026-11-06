#pragma once

// AI PLAY Phase C0: small, platform-independent behavior-cloning brain.
// It learns button-state decisions from persisted human demonstrations. It abstains
// when the nearest examples are sparse or contradictory; it never invents certainty.

#include "mod/ai_play/learning.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace aiplay::brain {

enum class Decision : std::uint8_t { Abstain, Release, Press };

struct TrainingExample {
    Mechanic mechanic = Mechanic::Unknown;
    double verticalVelocity = 0.0;
    double vehicleSize = 1.0;
    double hazardGap = 100000.0;
    double hazardVerticalOffset = 0.0;
    std::uint32_t nearbyHazardCount = 0;
    bool upsideDown = false;
    bool buttonHeldBefore = false;
    bool targetHeld = false;
    double weight = 1.0;
};

struct Prediction {
    Decision decision = Decision::Abstain;
    double confidence = 0.0;
    double distance = 1.0;
    std::size_t neighbors = 0;
};

struct Model {
    std::vector<TrainingExample> examples;
    // Cached action balance per mechanic: [mechanic][release/press].
    std::array<std::array<std::size_t, 2>, 8> actionCounts{};
    std::uint64_t clearExamples = 0;
    std::uint64_t deathExamples = 0;
};

inline double featureDistance(const TrainingExample& a, const TrainingExample& b) {
    if (a.mechanic != b.mechanic) return 1000.0;
    const double velocity = (a.verticalVelocity - b.verticalVelocity) / 40.0;
    const double size = (a.vehicleSize - b.vehicleSize) / 2.0;
    const double gap = (a.hazardGap - b.hazardGap) / 180.0;
    const double offset = (a.hazardVerticalOffset - b.hazardVerticalOffset) / 120.0;
    const double count = (static_cast<double>(a.nearbyHazardCount) -
                          static_cast<double>(b.nearbyHazardCount)) / 8.0;
    const double gravity = a.upsideDown == b.upsideDown ? 0.0 : 1.0;
    const double held = a.buttonHeldBefore == b.buttonHeldBefore ? 0.0 : 0.75;
    return std::sqrt(velocity * velocity + size * size + gap * gap +
                     offset * offset + count * count + gravity * gravity + held * held);
}

inline bool parseTrainingFile(const std::string& path, Model& model) {
    std::ifstream in(path);
    if (!in) return false;
    std::string magic;
    unsigned version = 0;
    if (!(in >> magic >> version) || magic != "AIPLAY_DEMOS" || version != 1)
        return false;

    std::string line;
    std::getline(in, line); // consume header newline
    Model parsed;
    constexpr std::size_t kMaxModelExamples = 2048;
    std::size_t replacementCursor = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream row(line);
        char record = 0;
        std::string level;
        int completed = 0, upsideDown = 0, heldBefore = 0, down = 0;
        double progress = 0.0;
        std::uint64_t tick = 0;
        unsigned mechanic = 0, hazardCount = 0;
        TrainingExample e;
        std::int32_t hazardType = -1;
        double x = 0.0, y = 0.0;
        if (!(row >> record >> level >> completed >> progress >> tick >> mechanic
                  >> x >> y >> e.verticalVelocity >> e.vehicleSize >> e.hazardGap
                  >> e.hazardVerticalOffset >> hazardType >> hazardCount
                  >> upsideDown >> heldBefore >> down)) continue;
        if (record != 'A' || mechanic >= static_cast<unsigned>(Mechanic::Unknown) ||
            completed < 0 || completed > 1 || progress < 0.0 || progress > 1.0 ||
            !std::isfinite(e.verticalVelocity) || !std::isfinite(e.vehicleSize) ||
            !std::isfinite(e.hazardGap) || !std::isfinite(e.hazardVerticalOffset) ||
            (upsideDown != 0 && upsideDown != 1) ||
            (heldBefore != 0 && heldBefore != 1) || (down != 0 && down != 1))
            continue;
        e.mechanic = static_cast<Mechanic>(mechanic);
        if (!isPlayableMechanic(e.mechanic)) continue;
        e.nearbyHazardCount = std::min(hazardCount, 100u);
        e.upsideDown = upsideDown != 0;
        e.buttonHeldBefore = heldBefore != 0;
        e.targetHeld = down != 0;
        ++parsed.actionCounts[static_cast<std::size_t>(e.mechanic)][e.targetHeld ? 1 : 0];
        // A clear is stronger evidence, while actions from a failed run still
        // describe the trajectory before the failure and remain useful at lower weight.
        e.weight = completed ? 1.0 : 0.55 + 0.25 * clamp01(progress);
        if (parsed.examples.size() < kMaxModelExamples) {
            parsed.examples.push_back(e);
        } else {
            // Keep a bounded rolling sample so later demonstrations are not ignored.
            parsed.examples[replacementCursor] = e;
            replacementCursor = (replacementCursor + 1) % kMaxModelExamples;
        }
        if (completed) ++parsed.clearExamples;
        else ++parsed.deathExamples;
    }
    model = std::move(parsed);
    return true;
}

inline void trainFromFile(Model& model, const std::string& path) {
    Model fresh;
    if (parseTrainingFile(path, fresh)) model = std::move(fresh);
}

inline TrainingExample describe(const LiveObservation& observation) {
    TrainingExample e;
    e.mechanic = observation.mechanic;
    e.verticalVelocity = observation.verticalVelocity;
    e.vehicleSize = observation.vehicleSize;
    e.hazardGap = observation.nearestHazardGap;
    e.hazardVerticalOffset = observation.nearestHazardVerticalOffset;
    e.nearbyHazardCount = observation.nearbyHazardCount;
    e.upsideDown = observation.upsideDown;
    e.buttonHeldBefore = observation.buttonHeld;
    return e;
}

inline Prediction predict(const Model& model, const LiveObservation& observation) {
    Prediction result;
    if (!observation.synchronized || observation.dead ||
        !isPlayableMechanic(observation.mechanic) || model.examples.size() < 3)
        return result;

    const auto query = describe(observation);
    struct Neighbor { double distance = 0.0; double weight = 0.0; bool held = false; };
    // Fixed-size top-k buffer: no per-frame heap allocation and no full sort.
    constexpr std::size_t k = 5;
    std::array<Neighbor, k> nearest{};
    std::size_t count = 0;
    for (const auto& example : model.examples) {
        if (example.mechanic != query.mechanic) continue;
        const double distance = featureDistance(query, example);
        if (!std::isfinite(distance)) continue;
        if (count < k) {
            nearest[count++] = {distance, example.weight, example.targetHeld};
            continue;
        }
        std::size_t worst = 0;
        for (std::size_t i = 1; i < k; ++i)
            if (nearest[i].distance > nearest[worst].distance) worst = i;
        if (distance < nearest[worst].distance)
            nearest[worst] = {distance, example.weight, example.targetHeld};
    }
    if (count < 3) return result;

    double press = 0.0, release = 0.0, total = 0.0;
    double bestDistance = nearest[0].distance;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& n = nearest[i];
        bestDistance = std::min(bestDistance, n.distance);
        const double w = n.weight / (0.04 + n.distance * n.distance);
        (n.held ? press : release) += w;
        total += w;
    }
    if (total <= 0.0) return result;
    result.neighbors = count;
    result.distance = bestDistance;
    result.confidence = std::max(press, release) / total;
    // Conservative abstention: do not act on distant or conflicting demonstrations.
    if (result.distance > 1.35 || result.confidence < 0.72) return result;
    result.decision = press > release ? Decision::Press : Decision::Release;
    return result;
}

} // namespace aiplay::brain
