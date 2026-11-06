#include "mod/ai_play/controller.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    using namespace aiplay;
    using namespace aiplay::brain;
    using namespace aiplay::controller;

    const auto path = std::filesystem::temp_directory_path() / "aiplay_live_policy_test.dat";
    {
        std::ofstream out(path, std::ios::trunc);
        out << "AIPLAY_DEMOS 1\n";
        for (unsigned mechanic = 0; mechanic < 8; ++mechanic) {
            for (int i = 0; i < 12; ++i) {
                // Press and release examples occupy distinct, repeatable states.
                out << "A 91 1 1.0 " << (1000 + mechanic * 100 + i)
                    << ' ' << mechanic
                    << " 0 0 " << -10.0 - i * 0.01
                    << " 1 30 0 8 2 0 0 1\n";
            }
            for (int i = 0; i < 12; ++i) {
                out << "A 91 1 1.0 " << (2000 + mechanic * 100 + i)
                    << ' ' << mechanic
                    << " 0 0 " << 10.0 + i * 0.01
                    << " 1 30 0 8 2 0 1 0\n";
            }
        }
    }

    Model model;
    trainFromFile(model, path.string());
    for (const auto mechanic : kPlayableMechanics) {
        const auto index = static_cast<std::size_t>(mechanic);
        assert(model.actionCounts[index][0] == 12);
        assert(model.actionCounts[index][1] == 12);
        assert(readyFor(model, mechanic));

        LiveObservation press;
        press.mechanic = mechanic;
        press.verticalVelocity = -10.0;
        press.vehicleSize = 1.0;
        press.nearestHazardGap = 30.0;
        press.nearestHazardVerticalOffset = 0.0;
        press.nearbyHazardCount = 2;
        press.buttonHeld = false;
        press.synchronized = true;
        auto pressResult = decideDetailed(model, press, false, true);
        assert(pressResult.reason == Reason::ActionIssued);
        assert(pressResult.command.valid && pressResult.command.down);

        LiveObservation release = press;
        release.verticalVelocity = 10.0;
        release.buttonHeld = true;
        auto releaseResult = decideDetailed(model, release, true, true);
        assert(releaseResult.reason == Reason::ActionIssued);
        assert(releaseResult.command.valid && !releaseResult.command.down);
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    return 0;
}
