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

The first Android milestone is one reliable observation/input loop shared by all eight standard modes, not eight unrelated controllers or an ambitious learning score.

## 9. Delivery phases

### Phase A — Learning data model
Define versioned global and per-level records, mechanic identifiers, attempt summaries, and unit tests for persistence and updates.

### Phase B1 — Persistent starter memory and human-play telemetry
Implemented a versioned local memory file for the First Insane Demon starting prior and per-level attempt/best-progress records. Ordinary human deaths and clears are recorded; solver/replay-driven sessions and start-position test runs are excluded. This is progress telemetry only: it does not infer mechanic mastery from a percentage.

### Phase B2 — Shared all-mode observation adapter
The first read-only adapter now samples the live player during ordinary human play for Cube, Ship, Ball, UFO, Wave, Robot, Spider, and Swing. It records tick, position, vertical velocity, vehicle size, gravity orientation, held-button state, and a short-range hazard summary from Geometry Dash's own damaging-object query in a bounded in-session buffer. The sensor runs at 10 Hz and immediately on mode transitions. Contradictory mode flags are classified as Unknown. Solver/replay sessions, test-mode layers, dead-player states, and unsynchronized observations are excluded. The hook opts in only after a successful non-replay, non-test PlayLayer initialization. Ordinary player-1 press/release edges are also captured with their tick and active mode as bounded demonstration data; solver injections and replay input are excluded. These demonstrations and sensor samples are telemetry only; they do not increase skill until paired with policy predictions and verified outcomes.

Still required: Android 2.2081 validation, richer hazard/context features for planning, and a policy-controlled press/release adapter with safeguards for pause, death, restart, and loss of synchronization.

### Phase C — All-mode observation and action adapter
Implement one shared live adapter that identifies Cube, Ship, Ball, UFO, Wave, Robot, Spider, and Swing, captures the state needed by their models, and sends ordinary press/release input. Each mode gets its own timing/control model and validation cases; unknown or desynchronized states are never trained.

### Phase D — All-mode transferable learning
Bring all eight mode models through the same observe → predict → act → verify → update → persist loop. Keep separate evidence and confidence per mechanic, while sharing reusable timing/pattern representations where the evidence supports transfer. Validate each mode on Android 2.2081; do not count solver output as learned policy skill.

### Phase E — AI PLAY interface
Add the AI PLAY entry point, learning stage, per-mechanic skill view, level-specific progress, and the requested click-sound settings. UI must distinguish verified learning from unverified estimates.

## 10. Acceptance criteria

AI PLAY's first meaningful learning release is acceptable only when:

1. Each of the eight standard modes has an independent skill record, and a skill learned on Level A is loaded and used on Level B.
2. The agent does not relearn a mastered basic mechanic from zero on the next level.
3. A new pattern can still cause uncertainty and trigger adaptation.
4. Skill changes are traceable to verified observations.
5. Attempt count alone never increases skill.
6. The level-specific memory and global skill memory remain separate after restart.
7. Android 2.2081 behavior is tested on-device; Windows-only evidence is not presented as Android validation.
8. Every action is an ordinary game input and the game's own state decides whether it succeeded.

## 11. Current implementation status

The existing repository is a solver-first codebase. The starting-skill prior, platform-independent learning model for all eight mode identifiers, local persistence, basic human-play attempt/progress telemetry, strict all-mode classification, and a bounded read-only live player-state sampler are implemented. The sampler and human-input capture are not a playing policy and their telemetry does not update skill. Policy-controlled input, richer context modeling, verified mechanic learning, AI PLAY button, sound profiles, and Android validation remain outstanding.


## Automatic human-play demonstration collection

On entry to a normal, non-replay, non-test level, AI PLAY enables its read-only observation and player-1 action capture automatically. At each human death or clear, the attempt's action demonstrations are appended to `ai_play_training_v1.dat` in the Geode mod save directory. The file is capped at 16 MiB; when the cap is exceeded, a fresh dataset is started with the current attempt. This is persistent imitation-learning data collection, not yet an online-trained policy: global mechanic skill is not updated from raw demonstrations, and no autonomous inputs are generated.


## Behavior-cloning brain — Phase C0

A first platform-independent policy core is now present in `src/mod/ai_play/brain.hpp`. It reads the persisted `AIPLAY_DEMOS 1` action records and builds a compact nearest-neighbor behavior-cloning model. The state descriptor uses active mechanic, vertical velocity, vehicle size, nearest-hazard gap and vertical offset, nearby-hazard count, gravity orientation, and prior held-button state; raw level X/Y coordinates are intentionally excluded so examples can transfer between levels.

The policy returns a target button state only when it has at least three same-mechanic neighbors, the closest example is within a conservative distance threshold, and the weighted vote is sufficiently consistent. Otherwise it returns `Abstain`. Clear-run examples receive more weight than death-run examples. The model is rebuilt after each attempt, so newly saved demonstrations can affect the next attempt without restarting the game.

This is the first actual inference component, not yet an autonomous player. The game hook does not yet call this policy to issue inputs; there is no restart controller, pause/death release guard, or Android-validated control loop. A sparse demonstration file will correctly produce abstentions rather than a claim of Extreme Demon capability.


## Offline controller — Phase C1 (guarded)

Added `src/mod/ai_play/controller.hpp` and a controller test. The policy is entirely local: no API key, network request, remote model, or paid inference service. It only arms a mechanic after at least 24 demonstrations for that mechanic, including at least 8 press and 8 release labels. The game hook asks the model for a button-state decision and sends only normal player-1 press/release edges through the existing game input handler. It never changes physics, collision, player position, or hitboxes.

Safety gates exclude solver sessions, test/replay layers, and takeover sessions. A model abstention, death, untrained mechanic, or unsafe session releases any AI-held button. Leaving the level also attempts a final release. Control is therefore automatic only after enough human demonstrations exist for the active mechanic; a fresh install will not pretend to know how to beat an unfamiliar level.

**Not completed / not claimed:** a dedicated autonomous restart manager is not added. The controller does not yet generate new training examples from its own actions or update weights from self-play outcomes. It cannot promise Extreme Demon clears. Android runtime and the Geode hook have not been built or device-tested; treat this as experimental code until that validation is done.


## Outcome-gated AI rollouts — Phase C2 (experimental)

AI button edges are recorded into a bounded, separate in-memory rollout buffer. The buffer is not mixed with human demonstrations. When Geometry Dash confirms a genuine level clear, that successful AI trajectory is appended as positive demonstration data and the local nearest-neighbor model is rebuilt. When the AI dies, its trajectory is discarded; the model is not taught that the failed actions were correct. This avoids the most direct form of self-labeling feedback.

This is outcome-gated imitation, not a complete reinforcement-learning algorithm: it does not estimate counterfactual action values, assign delayed credit to every action, or autonomously restart after death. A successful trajectory is only positive evidence for the sequence as a whole. Autonomous retry remains disabled until a supported Android 2.2081 restart path and pause/quit/attempt-limit behavior can be validated. No Android build or device test is claimed.
