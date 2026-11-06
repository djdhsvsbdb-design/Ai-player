#include "mod/ai_play/brain.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    using namespace aiplay;
    using namespace aiplay::brain;
    const auto path = std::filesystem::temp_directory_path() / "aiplay_brain_test.dat";
    {
        std::ofstream out(path, std::ios::trunc);
        out << "AIPLAY_DEMOS 1\n";
        // Three similar Wave decisions with consistent release labels.
        for (int i = 0; i < 3; ++i)
            out << "A 77 1 1.0 " << (100 + i) << " 4 "
                << (10 + i) << " 20 0.5 1 24 0 8 1 0 1 0\n";
        // A different mechanic must not contaminate the Wave policy.
        out << "A 77 1 1.0 200 1 10 20 0.5 1 24 0 8 1 0 0 1\n";
    }
    Model model;
    trainFromFile(model, path.string());
    assert(model.examples.size() == 4);
    assert(model.clearExamples == 4);
    assert(model.actionCounts[static_cast<std::size_t>(Mechanic::Wave)][0] == 3);
    assert(model.actionCounts[static_cast<std::size_t>(Mechanic::Ship)][1] == 1);
    LiveObservation observation;
    observation.mechanic = Mechanic::Wave;
    observation.verticalVelocity = 0.5;
    observation.vehicleSize = 1;
    observation.nearestHazardGap = 24;
    observation.nearestHazardVerticalOffset = 0;
    observation.nearbyHazardCount = 1;
    observation.buttonHeld = true;
    observation.synchronized = true;
    auto prediction = predict(model, observation);
    assert(prediction.neighbors == 3);
    assert(prediction.decision == Decision::Release);
    assert(prediction.confidence >= 0.72);

    observation.mechanic = Mechanic::Ship;
    assert(predict(model, observation).decision == Decision::Abstain);
    observation.mechanic = Mechanic::Wave;
    observation.synchronized = false;
    assert(predict(model, observation).decision == Decision::Abstain);
    observation.synchronized = true;
    observation.dead = true;
    assert(predict(model, observation).decision == Decision::Abstain);
    // The inference model must stay bounded even when the persisted log is large.
    const auto largePath = std::filesystem::temp_directory_path() / "aiplay_brain_large_test.dat";
    {
        std::ofstream out(largePath, std::ios::trunc);
        out << "AIPLAY_DEMOS 1\n";
        for (int i = 0; i < 2100; ++i)
            out << "A 77 1 1.0 " << (300 + i) << " 4 "
                << "10 20 0.5 1 24 0 8 1 0 1 0\n";
    }
    Model bounded;
    trainFromFile(bounded, largePath.string());
    assert(bounded.examples.size() == 2048);
    std::error_code ec;
    std::filesystem::remove(largePath, ec);
    std::filesystem::remove(path, ec);
    return 0;
}
