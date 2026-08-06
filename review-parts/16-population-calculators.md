## Population — calculators

**Files:** `src/game/population/calculators/GoldenAgeCalculator.cpp`, `include/game/population/calculators/GoldenAgeCalculator.h`, `src/game/population/calculators/GrowthCalculator.cpp`, `include/game/population/calculators/GrowthCalculator.h`, `src/game/population/calculators/PopCompositionCalculator.cpp`, `include/game/population/calculators/PopCompositionCalculator.h`, `src/game/population/calculators/PopTypeAvailabilityCalculator.cpp`, `include/game/population/calculators/PopTypeAvailabilityCalculator.h`, `src/game/population/calculators/RiotCalculator.cpp`, `include/game/population/calculators/RiotCalculator.h`

**Assessment:** `GrowthCalculator` and `GoldenAgeCalculator` are small, readable, and (for growth) already hardened by the prior 3.3 fix — `GrowthRate ≤ 0` blocks threshold growth explicitly, and the NearZeroGrowth/PopulationBoom gap is an honest TODO. The dominant weakness is `RiotCalculator`'s unfinished state machine (`ForceRiot` vs `Update`) and `PopTypeAvailabilityCalculator`'s obsolescence walk, which is order-dependent, non-transitive, and unbounded — fine for today's linear specialist chain, brittle for mods.

### [H] `ForceRiot` and `Update` disagree on what keeps a riot alive
`src/game/population/calculators/RiotCalculator.cpp:21-41` — `ForceRiot` sets `m_bRioting` and emits `OnIsRioting`, but `Update` clears that flag whenever `ComputeCondition_` is false. Probe Incite Drone Riots calls `ForceRiot` without changing composition (`src/game/units/ProbeActionEffects.cpp:137-139`), so a base that does not already have more drones than talents is only "rioting" until the next `Update`. `IsRioting` is already live for morale and probe rules (`MoraleCalculator.cpp`, `ProbeRules.cpp`); prior finding 5 noted `CheckRiotEndOfTurn` has no caller, but the calculator itself ships both halves of the machine. Wiring the existing EOT hook will silently undo every forced riot that natural counts would not sustain. Fix: sticky forced state (or a turn counter) that `Update` cannot clear until expiry, distinct from the drone>talent condition.

### [M] `ResolveCurrentType` can hang on an obsolescence cycle
`src/game/population/calculators/PopTypeAvailabilityCalculator.cpp:65-94` — the `while (bChanged)` loop follows `obsoletes` edges with no visited set and no step limit. Stock data is a DAG (Doctor→Empath→Transcend), but a mod that makes A obsolete B and B obsolete A never exits. Throw after detecting a repeat of `resolvedId` (or after `GetAll().size()` steps).

### [M] Availability obsolescence is one level deep and order-tied
`src/game/population/calculators/PopTypeAvailabilityCalculator.cpp:41-56` — `GetAvailable` only marks ids listed in `obsoletes` of types that are themselves currently available. `Transcend` obsoletes Empath/Thinker, not Doctor (`config/pop_types.json:159`); if Transcend's tech is present without Empath's, Doctor stays in the assignable list beside Transcend, and `ResolveCurrentType("Doctor", …)` (`:65-94`) also stays on Doctor because only Empath claims that edge. Stock play with a normal tech order hides this (Empath is available and strips Doctor), but any grant/mod that skips the middle tech desyncs UI availability from "current" specialist. Fix: close obsolescence under the available set (or walk the same chain `ResolveCurrentType` uses) and, when several successors obsolete one id, pick by an explicit rule rather than `GetAll()` order.

### [M] Composition calculator silently floors negative Lua results to zero
`src/game/population/calculators/PopCompositionCalculator.cpp:32-33` — after `EvalInt`, negative drone/talent targets become 0 with no error. That contradicts the project preference for throwing on bad values and stacks with `LuaRuntime::EvalInt` already returning 0 on formula errors (prior finding 3.6). A typo that yields −1 is indistinguishable from "no drones." Reject negatives (throw) or at least surface them; do not normalize.

### [M] No direct tests for three of five calculators
`RiotCalculator`, `GoldenAgeCalculator`, and `PopTypeAvailabilityCalculator` have no dedicated tests under `tests/` (only `GrowthCalculator` and composition-via-manager are covered). The ForceRiot sticky-state bug and the Doctor/Transcend obsolescence gap would both have been caught by a few focused cases on the calculator APIs alone.

### [L] Convention and hygiene items
- `include/game/population/calculators/PopCompositionCalculator.h:13-26` — `PopCompositionInputs` / `PopCompositionResult` are data structs without the required `_t` suffix; same for `RiotConditionInputs` (`RiotCalculator.h:11`).
- `include/game/population/calculators/PopCompositionCalculator.h:45-46`, `PopTypeAvailabilityCalculator.h:27` — collaborators stored as pointers after being taken as references in the constructor; `GoldenAgeCalculator` / `RiotCalculator` correctly keep `Signal<>&`.
- `include/game/population/calculators/RiotCalculator.h:15` — `targetTalents = -1` sentinel for "use talentCount"; an `std::optional<int>` (or a clearer inputs split) matches project style better than a magic negative.
- `src/game/population/calculators/GrowthCalculator.cpp:27` — `baseSize * nutrientsPerPop` multiplies as `int` before dividing by a `double`; cast either operand to `double` first so a large modded size cannot overflow before the scale is applied.
- `src/game/population/calculators/PopTypeAvailabilityCalculator.cpp:31,74` — repeated linear `std::find` over discovered techs; build an `unordered_set` once per call (same pattern both methods).

**Observed outside slice:**
- `src/game/faction/base/population/PopulationManager.cpp:243-248` — golden-age `workerCount` comes from `GetWorkerCount()`, which includes drones and talents (see review part 11); calculator formula is fine if plain workers are passed.
- `PopulationManager::CheckRiotEndOfTurn` / `CheckGoldenAgeEndOfTurn` still have no callers (prior finding 5; incomplete relative to live `ForceRiot`).
- `docs/architecture/faction-system.md:266-267` — still describes `GrowthCalculator` as stateful Lua `threshold_formula`; implementation is static `nutrientsPerPop` / `GrowthRate`.
