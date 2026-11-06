#include "mod/ai_play/runtime.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>

int main() {
    using namespace aiplay;
    namespace fs = std::filesystem;

    const auto path = fs::temp_directory_path() / "aiplay_runtime_test.dat";
    std::error_code ec;
    fs::remove(path, ec);
    fs::remove(fs::path(path.string() + ".tmp"), ec);

    runtime::load(path);
    assert(runtime::g_globalSkill.mechanics.size() == 8);
    runtime::beginLevel(12345);
    runtime::setLiveObservationEnabled(true);
    LiveObservation cube;
    cube.tick = 1;
    cube.mechanic = Mechanic::Cube;
    cube.synchronized = true;
    assert(runtime::shouldSampleLiveObservation(cube.tick, cube.mechanic));
    runtime::recordLiveObservation(cube);
    // Same-mode samples are rate-limited, but a mode transition is captured at once.
    cube.tick = 2;
    assert(!runtime::shouldSampleLiveObservation(cube.tick, cube.mechanic));
    runtime::recordLiveObservation(cube);
    cube.tick = 3;
    cube.mechanic = Mechanic::Wave;
    assert(runtime::shouldSampleLiveObservation(cube.tick, cube.mechanic));
    runtime::recordLiveObservation(cube);
    assert(runtime::g_liveObservations.size() == 2);
    auto rejected = cube;
    rejected.tick = 4;
    rejected.synchronized = false;
    runtime::recordLiveObservation(rejected);
    rejected.synchronized = true;
    rejected.mechanic = Mechanic::Unknown;
    runtime::recordLiveObservation(rejected);
    assert(runtime::g_liveObservations.size() == 2);
    runtime::setLiveObservationEnabled(false);
    rejected.tick = 30;
    rejected.mechanic = Mechanic::Ship;
    runtime::recordLiveObservation(rejected);
    assert(runtime::g_liveObservations.size() == 2);
    HumanActionSample blockedAction;
    blockedAction.tick = 30;
    blockedAction.mechanic = Mechanic::Ship;
    blockedAction.down = true;
    runtime::recordHumanAction(blockedAction);
    assert(runtime::g_humanActions.size() == 2);
    runtime::recordHumanAttempt(0.99, false); // replay/disabled sessions must not be counted
    assert(runtime::g_levels.at("12345").attempts == 0);
    runtime::setLiveObservationEnabled(true);

    HumanActionSample demo;
    demo.tick = 42;
    demo.mechanic = Mechanic::Wave;
    demo.x = 123.5;
    demo.y = 456.25;
    demo.down = true;
    runtime::recordHumanAction(demo);
    runtime::recordHumanAttempt(0.42, false);
    runtime::recordHumanAttempt(1.0, true);
    runtime::g_aiScore.points = 17;
    runtime::save();
    const auto& before = runtime::g_levels.at("12345");
    assert(before.attempts == 2);
    assert(before.bestProgress == 1.0);
    assert(before.observations.size() == 2);
    const auto trainingPath = path.parent_path() / "ai_play_training_v1.dat";
    {
        std::ifstream training(trainingPath);
        std::string contents((std::istreambuf_iterator<char>(training)), {});
        assert(contents.find("AIPLAY_DEMOS 1") != std::string::npos);
        assert(contents.find("A 12345 0 0.42 42") != std::string::npos);
    }
    assert(runtime::g_globalSkill.mechanics.at(Mechanic::Wave).overall.verifiedSamples == 0);

    // Simulate a fresh process and verify the profile and per-level experience survive.
    runtime::g_aiScore = score::ScoreBoard{};
    runtime::g_globalSkill = GlobalSkillMemory{};
    runtime::g_levels.clear();
    runtime::g_liveObservations.clear();
    runtime::g_humanActions.clear();
    runtime::g_liveObservationEnabled = false;
    runtime::g_currentLevelKey.clear();
    runtime::g_memoryPath.clear();
    runtime::g_loaded = false;
    runtime::load(path);
    assert(runtime::g_globalSkill.mechanics.size() == 8);
    assert(runtime::g_aiScore.points == 17);
    assert(runtime::g_globalSkill.mechanics.at(Mechanic::Wave).overall.value == 0.60);
    assert(runtime::g_levels.at("12345").attempts == 2);
    assert(runtime::g_levels.at("12345").bestProgress == 1.0);

    fs::remove(path, ec);
    fs::remove(fs::path(path.string() + ".tmp"), ec);
    return 0;
}
