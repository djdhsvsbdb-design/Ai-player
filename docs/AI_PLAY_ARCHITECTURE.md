# AI PLAY — Learning Architecture

> Status: design baseline. This document describes the intended AI PLAY learning layer; it does not claim that the learning system or Android runtime integration is implemented yet.

## 1. Product goal

AI PLAY is a persistent game-playing agent for Geometry Dash. It should improve through verified play attempts and carry transferable skills from one level to another.

The agent must:
- begin with a modest, measurable baseline rather than pretend to be an expert;
- act through ordinary in-game press/release input and let Geometry Dash determine the result;
- learn from successes, failures, and differences between predicted and observed outcomes;
- retain general mechanic knowledge across levels;
- retain level-specific experience separately;
- never increase its skill score merely because it has made more attempts.

A completed route from a solver is not, by itself, evidence that the agent learned to play. Solver output may be used as a bounded planning aid or diagnostic oracle during development, but it must not replace the agent's learning policy.

## 1.1 Starting skill preset: First Insane Demon

The initial profile is calibrated as a capable but inconsistent player whose hardest
completed level is approximately their first Insane Demon. It is intentionally below
expert/pro-level performance. The platform-independent model seeds modest priors for
Cube, Ship, Ball, UFO, Wave, Robot, Spider, and Swing, with weaker adaptation to unfamiliar
patterns than control of familiar mechanics.

These preset values are starting estimates only: they have zero verified samples and zero
confidence until live gameplay confirms them. They must not be displayed as proven mastery.
The agent can therefore begin with useful prior knowledge while still making mistakes,
adapting to new layouts, and improving through verified play across levels.

This is not a guarantee of completing every Insane Demon. Difficulty varies substantially
by level, and actual performance depends on the live observation/input adapter, physics
calibration, and on-device testing. The preset does not bypass gameplay or use teleportation,
noclip, or solver output as proof of player skill.

## 2. System boundaries

AI PLAY is split into five cooperating parts:

1. **Player Policy** — chooses the next press/release decision using the current observation, global skills, and level experience.
2. **Mechanic Models** — reusable knowledge for cube, ship, ball, UFO, wave, robot, spider, swing, gravity changes, portals, and other supported situations.
3. **Global Skill Memory** — stores transferable knowledge and calibrated confidence for mechanics and timing patterns.
4. **Level Experience Memory** — stores observations, mistakes, checkpoints, and local route knowledge for one level.
5. **Game Adapter** — observes the live game and sends normal press/release input. The game remains the authority for collision, death, and completion.

The existing DP solver remains a separate planning/verification component. AI PLAY may request a candidate or compare a prediction with a replay, but the policy owns decisions about what to try and what to learn.

## 3. Two levels of memory

### 3.1 Global Skill Memory

This memory is shared by every level. It contains mechanic-level knowledge, not coordinates or a memorized full route.

Suggested record per mechanic:

- mechanic_id: stable identifier such as wave or ship;
- skill_estimate: calibrated estimate in the range 0..1;
- confidence: how much verified evidence supports the estimate;
- timing_model: learned timing ranges and response tendencies;
- pattern_library: reusable situation descriptors and successful action patterns;
- failure_signatures: recurring prediction errors and their corrections;
- last_updated: versioned learning timestamp.

Example: after the agent learns the basic rhythm and control response of Wave, a later level containing Wave starts with that global knowledge. It still has to adapt to the new geometry and speed; it must not start from zero.

### 3.2 Level Experience Memory

This memory is keyed by a stable level identity and stores only local information:

- observed sections and mechanic transitions;
- local timing landmarks and hazard patterns;
- attempt summaries and verified progress;
- recurring local mistakes;
- confidence in local predictions;
- version information so stale observations can be invalidated.

Level memory must not leak raw object positions into global mechanic skill. The global layer learns reusable patterns; the level layer remembers where and how those patterns appeared in this particular level.

## 4. Learning loop

Each attempt follows the same evidence-based cycle:

1. **Observe** — capture the current player state and nearby relevant game context.
2. **Classify** — identify the active mechanic and a compact situation descriptor.
3. **Predict** — estimate the likely outcome of candidate press/release choices.
4. **Act** — choose one action through the Game Adapter.
5. **Verify** — compare the prediction with the actual game result.
6. **Explain** — classify the outcome as a timing error, mechanic-model error, unfamiliar pattern, or uncertain observation.
7. **Update** — update local experience and, only when justified, global skill memory.
8. **Consolidate** — promote a pattern to reusable knowledge only after it succeeds consistently in more than one context.

A single lucky success must not produce a large skill increase. A repeated failure should not automatically reduce general skill if the cause is a new level-specific obstacle or an unreliable observation.

## 5. Skill dimensions

The first version should track separate dimensions instead of one opaque XP number:

- **Timing** — accuracy of press/release timing relative to observed game events.
- **Control** — ability to predict the effect of input for the active mechanic.
- **Pattern recognition** — ability to recognize previously learned situation types.
- **Adaptation** — speed and reliability when a known mechanic appears in unfamiliar geometry.
- **Consistency** — repeatability across attempts and across different sections.

These dimensions are estimates with confidence, not claims about human ability. A UI may show a simple overall stage, but the underlying measurements must remain inspectable.

## 6. A conservative update rule

For every verified decision, record a prediction, the chosen action, and the observed outcome. Update the relevant estimate using a bounded, confidence-weighted rule.

A practical first implementation can use an exponentially weighted error estimate:

error_t = observed_t - predicted_t

new_error = (1 - alpha) * old_error + alpha * error_t

The learning rate alpha should be smaller when evidence is noisy and larger only for well-observed, repeated examples. Skill should be derived from measured prediction error, timing deviation, and repeatability over a rolling window—not from attempt count.

Keep separate evidence buckets for:
- familiar patterns in familiar contexts;
- familiar mechanics in new contexts;
- genuinely new mechanics or patterns.

This prevents a difficult new section from erasing a well-established Wave skill.

## 7. Persistence and compatibility

- Store global skill memory independently of any level save.
- Store level experience under a stable level key.
- Version every persisted schema and provide safe defaults for missing or older data.
- Write updates atomically where the platform allows it.
- Do not store personal account data; the learning data is local gameplay state.
- Make learning reset/export controls explicit in the eventual UI.

## 8. Android 2.2081 constraints

The target is Geometry Dash 2.2081 on Android with Geode. The repository's Android notes explicitly describe that target as experimental and warn that Windows physics measurements are not proof of Android behavior.

Therefore:
- do not assume Windows offsets, timing, or physics measurements are valid on Android;
- keep the learning policy independent of platform-specific hooks;
- isolate observation and input behind the Game Adapter;
- validate every adapter behavior on the actual Android 2.2081 build;
- do not mark a mechanic as learned from an unverified or desynchronized replay.

The first Android milestone is a reliable observation/input loop, not an ambitious learning score.

## 9. Delivery phases

### Phase A — Learning data model
Define versioned global and per-level records, mechanic identifiers, attempt summaries, and unit tests for persistence and updates.

### Phase B — Observation and action adapter
Connect the policy to real in-game state and ordinary press/release input on Android 2.2081. Add safeguards for pause, death, restart, and loss of synchronization.

### Phase C — First transferable mechanic
Implement one mechanic end-to-end (initially Cube or Wave): observe, predict, act, verify, update, persist, and reuse the learned model in a different level.

### Phase D — Broader mechanic coverage
Add the remaining mechanics incrementally, each with explicit validation cases and confidence thresholds.

### Phase E — AI PLAY interface
Add the AI PLAY entry point, learning stage, per-mechanic skill view, level-specific progress, and the requested click-sound settings. UI must distinguish verified learning from unverified estimates.

## 10. Acceptance criteria

AI PLAY's first meaningful learning release is acceptable only when:

1. A skill learned on Level A is loaded and used on Level B.
2. The agent does not relearn a mastered basic mechanic from zero on the next level.
3. A new pattern can still cause uncertainty and trigger adaptation.
4. Skill changes are traceable to verified observations.
5. Attempt count alone never increases skill.
6. The level-specific memory and global skill memory remain separate after restart.
7. Android 2.2081 behavior is tested on-device; Windows-only evidence is not presented as Android validation.
8. Every action is an ordinary game input and the game's own state decides whether it succeeded.

## 11. Current implementation status

This file is an architectural baseline only. The existing repository is a solver-first codebase. The starting-skill prior and platform-independent learning records are implemented as a standalone model. The Player Policy, persistent memory stores, live learning updates, AI PLAY button, sound profiles, and Android validation still need implementation.
