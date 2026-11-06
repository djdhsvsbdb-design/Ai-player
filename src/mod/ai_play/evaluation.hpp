#pragma once

// AI PLAY offline policy evaluation.
// This code is deliberately excluded from the live game loop. It builds a deterministic
// train/holdout split and reports both coverage and accuracy so abstentions are visible.

#include "mod/ai_play/brain.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iomanip>
#include <ostream>

namespace aiplay::evaluation {

struct Counts {
    std::size_t samples = 0;
    std::size_t evaluated = 0;
    std::size_t abstained = 0;
    std::size_t correct = 0;
    std::size_t incorrect = 0;
    std::size_t falsePress = 0;
    std::size_t falseRelease = 0;

    double coverage() const {
        return samples == 0 ? 0.0
            : static_cast<double>(evaluated) / static_cast<double>(samples);
    }

    double accuracyWhenActing() const {
        return evaluated == 0 ? 0.0
            : static_cast<double>(correct) / static_cast<double>(evaluated);
    }

    double endToEndAccuracy() const {
        return samples == 0 ? 0.0
            : static_cast<double>(correct) / static_cast<double>(samples);
    }
};

struct Report {
    Counts overall;
    std::array<Counts, 8> mechanics{};
    std::size_t trainingExamples = 0;
    std::size_t holdoutExamples = 0;
};

inline std::size_t mechanicIndex(Mechanic mechanic) {
    return isPlayableMechanic(mechanic)
        ? static_cast<std::size_t>(mechanic) : 8u;
}

inline void addResult(Counts& counts, const brain::TrainingExample& expected,
                      const brain::Prediction& prediction) {
    ++counts.samples;
    if (prediction.decision == brain::Decision::Abstain) {
        ++counts.abstained;
        return;
    }

    ++counts.evaluated;
    const bool predictedHeld = prediction.decision == brain::Decision::Press;
    if (predictedHeld == expected.targetHeld) {
        ++counts.correct;
    } else {
        ++counts.incorrect;
        if (predictedHeld) ++counts.falsePress;
        else ++counts.falseRelease;
    }
}

// Deterministic holdout evaluation. Every holdout row is excluded from training;
// the evaluation is offline and may be O(N * kNN-scan), so never call it per frame.
// The modulo split is reproducible, not a substitute for a separate-level test set.
inline Report evaluate(const brain::Model& source, std::size_t holdoutModulo = 5) {
    Report report;
    holdoutModulo = std::max<std::size_t>(2, holdoutModulo);

    brain::Model training;
    training.clearExamples = source.clearExamples;
    training.deathExamples = source.deathExamples;

    for (std::size_t i = 0; i < source.examples.size(); ++i) {
        if (i % holdoutModulo == 0) {
            ++report.holdoutExamples;
            continue;
        }
        training.examples.push_back(source.examples[i]);
    }
    report.trainingExamples = training.examples.size();

    for (const auto& example : training.examples) {
        if (!isPlayableMechanic(example.mechanic)) continue;
        ++training.actionCounts[static_cast<std::size_t>(example.mechanic)]
                               [example.targetHeld ? 1u : 0u];
    }

    for (std::size_t i = 0; i < source.examples.size(); ++i) {
        if (i % holdoutModulo != 0) continue;
        const auto& expected = source.examples[i];
        const auto index = mechanicIndex(expected.mechanic);
        if (index >= report.mechanics.size()) continue;

        LiveObservation observation;
        observation.mechanic = expected.mechanic;
        observation.verticalVelocity = expected.verticalVelocity;
        observation.vehicleSize = expected.vehicleSize;
        observation.nearestHazardGap = expected.hazardGap;
        observation.nearestHazardVerticalOffset = expected.hazardVerticalOffset;
        observation.nearbyHazardCount = expected.nearbyHazardCount;
        observation.upsideDown = expected.upsideDown;
        observation.buttonHeld = expected.buttonHeldBefore;
        observation.synchronized = true;

        const auto prediction = brain::predict(training, observation);
        addResult(report.overall, expected, prediction);
        addResult(report.mechanics[index], expected, prediction);
    }
    return report;
}

inline void writeCsv(std::ostream& out, const Report& report) {
    out << "mechanic,samples,evaluated,abstained,correct,incorrect,"
           "coverage,accuracy_when_acting,end_to_end_accuracy,false_press,false_release\n";
    auto write = [&out](const char* name, const Counts& c) {
        out << name << ',' << c.samples << ',' << c.evaluated << ','
            << c.abstained << ',' << c.correct << ',' << c.incorrect << ','
            << std::fixed << std::setprecision(4) << c.coverage() << ','
            << c.accuracyWhenActing() << ',' << c.endToEndAccuracy() << ','
            << c.falsePress << ',' << c.falseRelease << '\n';
    };
    write("all", report.overall);
    constexpr const char* names[] = {
        "cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"
    };
    for (std::size_t i = 0; i < report.mechanics.size(); ++i)
        write(names[i], report.mechanics[i]);
}

} // namespace aiplay::evaluation
