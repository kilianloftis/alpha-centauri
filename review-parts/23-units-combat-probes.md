## Units — combat, probes, conquest, morale

**Files:** `src/game/units/AttackRules.cpp`, `include/game/units/AttackRules.h`, `src/game/units/BaseConquestConfigParser.cpp`, `include/game/units/BaseConquestConfigParser.h`, `src/game/units/BaseConquestEffects.cpp`, `include/game/units/BaseConquestEffects.h`, `src/game/units/BaseConquestRules.cpp`, `include/game/units/BaseConquestRules.h`, `src/game/units/CombatResolver.cpp`, `include/game/units/CombatResolver.h`, `src/game/units/DisengageRules.cpp`, `include/game/units/DisengageRules.h`, `src/game/units/InterceptRules.cpp`, `include/game/units/InterceptRules.h`, `src/game/units/MoraleCalculator.cpp`, `include/game/units/MoraleCalculator.h`, `src/game/units/MoraleConfig.cpp`, `include/game/units/MoraleConfig.h`, `src/game/units/MoraleConfigParser.cpp`, `include/game/units/MoraleConfigParser.h`, `src/game/units/ProbeActionConfigParser.cpp`, `include/game/units/ProbeActionConfigParser.h`, `src/game/units/ProbeActionEffects.cpp`, `include/game/units/ProbeActionEffects.h`, `src/game/units/ProbeActionExecutor.cpp`, `include/game/units/ProbeActionExecutor.h`, `src/game/units/ProbeRules.cpp`, `include/game/units/ProbeRules.h`, `src/game/units/ProbeTarget.cpp`, `include/game/units/ProbeTarget.h`, `include/game/units/BaseConquestConfig.h`, `include/game/units/ProbeActionResult.h`, `include/game/units/ProbeActionConfig.h`

**Assessment:** The slice is thoughtfully layered — pure predicates (`AttackRules`, `BaseConquestRules`, `DisengageRules`, `ProbeRules`) stay separate from world mutation (`CombatResolver`, `BaseConquestEffects`, `ProbeActionEffects`) and orchestration (`ProbeActionExecutor`). Headers document invariants clearly, and config drives conquest/morale/probe tunables. The dominant weakness is incomplete wiring of already-declared combat/probe mechanics (disengage chance, intercept source identity, sabotage/deploy bookkeeping), so stock config values and effect conditions can silently disagree with runtime behavior.

### [H] Roll `DisengageChance` before committing a withdrawal
`src/game/units/CombatResolver.cpp:45-75` — `TryDisengage_` moves the unit as soon as eligibility, half-HP, and a retreat tile exist; it never reads `StatId_t::DisengageChance`. Stock Speeder chassis grants `disengage_chance: 25` (`config/unit_components/chassis.json:41`), and `docs/game-rules/unit-components.md` defines that stat as a percent chance, so eligible units always withdraw. Roll the resolved chance (e.g. via `RollPercent`) after the half-HP gate and before `MoveUnit`.

### [H] Keep the originating base on building intercept candidates
`src/game/units/InterceptRules.cpp:100-107` / `166-189` — ThisBase intercepts are collected from a specific `BaseManager`, but `InterceptCandidate_t` only stores `sourceId`. On fail-destruction, `MaybeDestroyInterceptSourceOnFail_` calls `Faction::FindBaseWithBuilding`, which returns the first base owning that building id. With the same building in two bases (or FactionGlobal charges), the wrong copy is destroyed and `NotifyBuildingDestroyed` clears a deploy against the wrong inventory. Store a `BaseManager*` (or base id) on the candidate when `sourceKind == Building` and destroy through that base.

### [M] Probe sabotage skips deploy-ledger notification and conflates facility vs random
`src/game/units/ProbeActionEffects.cpp:105-134` — `DestroyBuilding` is called without `Faction::NotifyBuildingDestroyed`, unlike `BaseConquestEffects` / intercept fail paths; a sabotaged ODP (or any cooling deploy source) leaves `m_buildingDeploys` stale so `CountReadyBuildings` under-counts. Separately, `SabotageFacility` with an empty `facilityId` falls through to the random/production-wipe branch, and a non-empty missing id still reports `ProbeDestroyedFacility_t` after `DestroyBuilding`'s documented no-op. Require a present building for targeted sabotage, notify the faction on every real destroy, and do not claim success when nothing was removed.

### [M] Intercept condition context marks the wrong combat role
`src/game/units/InterceptRules.cpp:93` — Candidates are filtered with `EffectContext_t{&rDefender.GetTile(), CombatRole_t::Attacker}` and never set `pAttacker`. `IsDefending` therefore always fails for intercept effects, and `AttackerIsEmbarked` is always false even though `UnitFilterSatisfied` already has the live attacker. Use `CombatRole_t::Defender` (or document a dedicated intercept role) and set `pAttacker = &rAttacker`.

### [M] Unit-subvert cost treats HQ-tile distance 0 as the no-HQ default
`src/game/units/ProbeRules.cpp:107-112` — `QuoteSubvertUnitCost_` replaces `distToHq <= 0` with `k_defaultHqDistance` (12), while the base mind-control quote returns `nullopt` for the same case (`88-95`). A unit on the HQ tile therefore uses denominator `12 + distBias` instead of being refused or priced at true distance 0 (`distBias` only) — with stock `dist_bias: 2` that makes HQ-garrison subversion much cheaper. Align with the base path (unavailable / nullopt at dist 0).

### [M] Escape-pod design failures fail closed without error
`src/game/units/BaseConquestEffects.cpp:121-171` — `EnsureEscapePodDesign_` returns `nullptr` when the registry is missing or any configured component id is unknown; `SpawnEscapePods_` then returns 0. Cross-species capture still strips population (`ApplySpeciesClashPopulation_`) but silently spawns no pods, violating the project preference to throw on unexpected null/config errors. Throw from ensure/spawn when `componentIds` is non-empty but assembly cannot proceed.

### [M] `risk_repeat` depends on a caller flag the executor never owns
`src/game/units/ProbeActionExecutor.cpp:94-96` / `156-165` — Repeat risk is selected only when the caller passes `bRepeatAtBase`. No in-slice state records prior missions at a base, so `risk_repeat` in `probe_actions.json` (e.g. steal tech) is inert unless every caller remembers. Track attempts on the target (or faction ledger) inside the probe pipeline, or drop the parameter and derive repeat from game state.

### [L] Convention and hygiene items
- `include/game/units/DisengageRules.h:22` — Header says “neither combatant is an air unit”; `DisengageRules.cpp:64-66` also blocks Orbital — update the contract comment.
- `include/game/units/ProbeActionConfig.h:35-68` — `ProbeActionIdToString` / `ParseProbeActionId` duplicate the snake_case map; keep one table.
- `src/game/units/ProbeActionEffects.cpp:118` — HQ exclusion uses string id `"Headquarters"` while `ProbeRules` already exposes `IsHeadquarters` via the rule flag.
- `src/game/units/AttackRules.cpp:5` / `ProbeActionEffects.cpp:9` — Likely unused includes (`BonusEffect.h`, `DiplomacyLedger.h`).
- `src/game/units/BaseConquestConfigParser.cpp:22-28` — Optional `json.value(..., default)` soft-fills tunables; other parsers in this slice throw on missing required structure.

**Observed outside slice:**
- `src/ui/world/WorldView.cpp:565-566` — `TryProbeAction` is invoked without `facilityId` or `bRepeatAtBase`, so facility sabotage and repeat-risk are unreachable from the current UI even if the executor API is fixed.
- `include/game/effects/EffectEnums.h` / chassis config — `DisengageChance` is a first-class stat with stock amounts; combat is the consumer that must honor it (finding above).
