#include "mod/ai_play/runtime.hpp"

#include <cassert>
#include <filesystem>

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
    LiveObservation cube;
    cube.tick = 1;
    cube.mechanic = Mechanic::Cube;
    cube.synchronized = true;
    runtime::recordLiveObservation(cube);
    // Same-mode samples are rate-limited, but a mode transition is captured at once.
    cube.tick = 2;
    runtime::recordLiveObservation(cube);
    cube.tick = 3;
    cube.mechanic = Mechanic::Wave;
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

    runtime::recordHumanAttempt(0.42, false);
    runtime::recordHumanAttempt(1.0, true);
    const auto& before = runtime::g_levels.at("12345");
    assert(before.attempts == 2);
    assert(before.bestProgress == 1.0);
    assert(before.observations.size() == 2);
    assert(runtime::g_globalSkill.mechanics.at(Mechanic::Wave).overall.verifiedSamples == 0);

    // Simulate a fresh process and verify the profile and per-level experience survive.
    runtime::g_globalSkill = GlobalSkillMemory{};
    runtime::g_levels.clear();
    runtime::g_liveObservations.clear();
    runtime::g_currentLevelKey.clear();
    runtime::g_memoryPath.clear();
    runtime::g_loaded = false;
    runtime::load(path);
    assert(runtime::g_globalSkill.mechanics.size() == 8);
    assert(runtime::g_globalSkill.mechanics.at(Mechanic::Wave).overall.value == 0.60);
    assert(runtime::g_levels.at("12345").attempts == 2);
    assert(runtime::g_levels.at("12345").bestProgress == 1.0);

    fs::remove(path, ec);
    fs::remove(fs::path(path.string() + ".tmp"), ec);
    return 0;
}
