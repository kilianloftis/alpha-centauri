# Full code review — remediation work packages

**Date:** 2026-08-06
**Source of findings:** [`docs/full-code-review.md`](full-code-review.md) (251 findings: H=50, M=163, L=38)
**Precedent:** [`docs/effects-fix-packages.md`](effects-fix-packages.md) → [`docs/effects-fix-prompts/`](effects-fix-prompts/) (packages 1–8, complete)
**Goal:** Group the remaining review findings into 17 attack packages. Each package below is self-contained enough for a dedicated analysis pass that verifies the findings against current code and produces a ready-to-paste AI implementation prompt.

**Project constraints (always apply):**
- Follow `.devin/rules/coding-guidelines.md` (SOLID, references over pointers, constructors produce valid objects, throw over silent defaults, no legacy/back-compat shims — update all call sites).
- Follow `.devin/rules/architecting.md` (keep `docs/architecture/` diagrams current when a component moves or appears).
- Build/test only via `./bd` (never raw cmake/make/ctest).
- Prefer config/Lua for moddability; do not hardcode game numbers or ids in C++.
- Do not invent SMAC rules. Leave TODOs for genuine unknowns.
- Unimplemented features are fine; silent wrong values and stubs wired to produce them are not.

**Standing caveat for every analysis pass:** the review was taken on 2026-08-03 and effects packages 1–8 have landed since. Line numbers have moved, several headers were renamed (`BonusEffect.h` → `EffectConfig.h`, `BonusEffectParser` → `EffectConfigParser`), and a few findings may already be partly fixed. Verify each finding at `path:line` before designing the fix; report any finding that no longer reproduces instead of "fixing" it.

**Suggested sequence:** 1 → 2 → 3 → 4 (contracts first), then 5–10 and 17 (domain correctness) in any order, then 11–13, then 14–15 (UI), 16 last.

---

## Package 1 — Turn pipeline integrity: hooks, exceptions, yield/resume

**Priority:** Highest. Two shipped configurations silently change behaviour; one throw wedges the game.
**Theme:** The turn pipeline either does what the config says or fails loudly; yield/resume must not re-run work.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | A configured replace hook silently deletes the stage's behavior | `TurnStages.h:67-71`, `HookContext.cpp:50-60` |
| [H] | An exception from a stage wedges TurnProcessor and skips the post hooks | `TurnProcessor.cpp` |
| [H] | Mid-pass `Yield` re-executes earlier units' multi-turn orders | `stages/PlayerActions.cpp` |
| [H] | WorldEvents forks a private RNG from year and map area alone | `stages/WorldEvents.cpp` |
| [M] | `repeat_for_each_faction` is inert for every built-in stage, with no cross-check | `TurnStageConfigParser` / stage kinds |
| [M] | `CreateStages` rediscovers the stage kind by RTTI and silently collapses duplicate ids | `TurnStageFactory.cpp` |
| [M] | Per-faction resume relies on an ordering invariant no one enforces | `TurnProcessor` |
| [M] | `m_phase` can skip the interaction gate after a yielded player is gone | `stages/PlayerActions.cpp` |
| [M] | Population stage never runs riot / golden-age end-of-turn updates | `stages/Population.cpp` |
| [M] | No tests for `PlayerActions` yield / resume semantics | tests |
| [M] | Custom stage "at least one hook" guard does not enforce its own invariant | `stages/CustomModStage` |
| [M] | First-playable-year epoch is hardcoded beside GameState's starting year | late stages |
| [M] | The turn-system architecture doc predates the yield/resume contract | `docs/architecture/turn-system.md` |
| [L] | Convention/hygiene blocks for the turn-pipeline and both turn-stage sections | — |

### Problem statement

`config/turn_stages.json` can already turn a built-in stage into a no-op: a `replace` hook entry skips `ExecuteImpl` and the hook system never loads a script, so the stage prints a line and returns `Continue`. Any throw from a stage leaves `TurnProcessor` mid-stage with post hooks unrun and no defined recovery. `PlayerActions` yields mid-pass and, on resume, walks units that already executed their multi-turn orders. `repeat_for_each_faction` is accepted and ignored. WorldEvents derives its RNG from year + map area, so the same year is identical across saves and unaffected by the session seed.

### Likely fix direction

- Decide the replace-hook contract: either reject a `replace` hook with no loadable script at config load, or make hook execution able to actually substitute behaviour. No silent deletion.
- Give `TurnProcessor` an explicit exception story: stage failure must run post hooks / restore a resumable state or abort the turn with a diagnostic, not wedge.
- Make resume idempotent — track which units executed this pass, or restructure `PlayerActions` so the pre-yield work is not re-entered.
- Route stage RNG through the session seed (one game-level seed source; see also package 4's `FactionFlavor` item).
- Honor or reject `repeat_for_each_faction`; make stage kind explicit rather than RTTI-derived; reject duplicate stage ids.

### Key files

`include/game/TurnStages.h`, `src/game/TurnProcessor.cpp`, `src/game/TurnStageFactory.cpp`, `src/game/TurnStageConfigParser.cpp`, `src/game/HookContext.cpp`, `src/game/stages/PlayerActions.cpp`, `stages/Population.cpp`, `stages/WorldEvents.cpp`, `config/turn_stages.json`, `docs/architecture/turn-system.md`, turn/stage tests.

### Risks / invariants

Changing yield semantics touches package 2 (UI gating) — agree the contract once, in this package, and let package 2 consume it. Do not implement Lua hook loading here unless analysis shows it is the smallest honest fix (that is package 16's mod-seam work).

### Analysis output path

`docs/full-review-fix-prompts/01-turn-pipeline-integrity.md`

---

## Package 2 — Modal / overlay contract and turn gating

**Priority:** Highest. Six [H] findings are the same missing rule, and the failure is player-facing.
**Theme:** One rule for "a modal is open": input capture, shortcut suppression, stacking, and whether the turn may advance.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Reconcile turn `Yield` with the UI lifetime contract | architecture; `Engine::ProcessTurn_` |
| [H] | Block global shortcuts from stacking overlays | `UIManager` |
| [H] | Prevent stacking multiple council proposal popups | `ui/commlinks` |
| [H] | Do not dismiss the vote view while a proposal is still pending | `ui/council/CouncilVoteView` |
| [H] | Capture input while orbital popups are open | `ui/satellite` |
| [H] | Gate turns and map input while WorldView modal popups are open | `ui/world/WorldView` |
| [M] | Make in-view selector popups modal | `ui/base` |
| [M] | Vote can stack multiple ballot popups | `ui/council` |
| [M] | Component selector is not modal; popups can stack | `ui/unit-designer` |
| [M] | Do not run ProcessTurn from inside Render | `ui/world/WorldView` |
| [M] | Outside-click dismiss is unreachable (ProductionSelector, proposals popup, probe/supply popups) | three UI sections |

### Problem statement

Every view invents its own answer to "am I modal?". Global shortcuts fire while overlays are open, popups stack on top of themselves, the council vote view can be closed with a pending vote (which then bricks the council — see package 5), and `WorldView` advances the turn from inside `Render` while `Engine::ProcessTurn_` still asserts turns are atomic. Outside-click dismissal is dead code in three popups because the click never reaches them.

### Likely fix direction

- Define one modal/overlay contract on `UIManager`: what "modal" means for input routing, shortcut suppression, push/replace of an already-open popup of the same kind, and turn advance permission.
- Make the turn-gate a query (`CanAdvanceTurn()` / "overlay depth") the engine asks, not an assertion it makes; reconcile with package 1's yield semantics.
- Move turn advance out of `Render` into the frame/update step.
- Fix outside-click dismissal once, in the shared popup path (coordinates with package 14's shared list-selector extraction).

### Key files

`include/ui/UIManager.h`, `src/ui/UIManager.cpp`, `src/game/Engine.cpp`, `src/ui/world/WorldView.cpp`, `src/ui/council/*`, `src/ui/commlinks/*`, `src/ui/satellite/*`, `src/ui/base/*`, `src/ui/unit-designer/*`, `docs/architecture/ui-system.md`.

### Risks / invariants

Do not fix each view locally — that is the current state. One contract, applied everywhere, with a test per view kind. Coordinate with package 14 (shared popup component) so the modal rule lives in the shared component, not in nine copies.

### Analysis output path

`docs/full-review-fix-prompts/02-modal-overlay-contract.md`

---

## Package 3 — Object lifetime and ownership-transfer protocol

**Priority:** High. Bases and units can now be destroyed and moved; nothing states who invalidates what.
**Theme:** One documented protocol for destroy / transfer, and every holder of a live reference obeys it.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Define a lifetime protocol now that bases can die or move | architecture; `ViewFactory::CreateBaseView` |
| [H] | Ownership transfer is implemented as destroy-then-recreate, inheriting destruction's side effects | `GameState` / `BaseConquestEffects` |
| [H] | `DestroyUnit` is also the transfer path, so transfers apply combat cargo rules | `UnitManager::DestroyUnit` |
| [M] | Building deploy cooldowns are never pruned and leak across base transfer | `GameState` / building deploy ledger |
| [M] | EventBridge wiring stays opt-in, and now has more call sites to forget | `EventBridge::WireBase` |
| [M] | The displaced-worker handler outlives the object it captures | `WorkerAssignmentManager` / `WorkedTileIndex` |
| [M] | `HomeBaseIndex` keeps three representations of one relation | `HomeBaseIndex` |
| [M] | `HomeBaseIndex` throws invariant violations from `noexcept` paths | same |

### Problem statement

`ExtractBase` / `TransferBaseTo` run from conquest, probes and diplomacy, but UI holds a raw `BaseManager&`, `EventBridge::WireBase` is called only on founding, deploy cooldowns keyed by base outlive the transfer, and unit transfer reuses the destruction path so cargo/escape-pod combat rules fire on a peaceful hand-over. `HomeBaseIndex` stores the same relation three ways and throws from `noexcept` members.

### Likely fix direction

- Write the protocol first (architecture doc): who may destroy, what must be detached first, what a transfer is allowed to preserve, and how UI/observers learn about it.
- Separate `TransferUnit` from `DestroyUnit`; separate "base changed owner" from "base destroyed".
- Give views a handle/observer that survives (or explicitly pops on) destruction, rather than a raw reference.
- Wire events in one place that both founding and transfer go through.
- Reduce `HomeBaseIndex` to one representation; make the invariant checks non-`noexcept` or make the paths not throw.

### Key files

`src/game/GameState.cpp`, `src/game/Faction.cpp`, `src/game/faction/UnitManager.cpp`, `src/game/units/BaseConquestEffects.cpp`, `src/game/units/ProbeActionEffects.cpp`, `src/game/faction/DiplomaticActionExecutor.cpp`, `src/game/EventBridge.cpp`, `include/ui/ViewFactory.h`, `include/game/faction/base/HomeBaseIndex.h`, `docs/architecture/high-level.md`.

### Analysis output path

`docs/full-review-fix-prompts/03-lifetime-and-transfer.md`

---

## Package 4 — Composition root, dependency validity, session boundaries

**Priority:** High. Five [H] findings; every new subsystem currently inherits the pattern.
**Theme:** Constructors produce valid objects; optional means optional everywhere; the composition root has explicit phases.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | `GameDataContext` is still a service locator: a default-constructed bag of nullable pointers | `GameDataContext.h/.cpp` |
| [H] | `Faction` is only half-constructed until `GameState::AddFaction` finishes wiring it | `GameState::AddFaction` |
| [H] | `GameState` and `Faction` are god-facades that every new subsystem must edit | both |
| [H] | The constructor accepts ten nullable dependencies with three different null behaviours | `BaseManager` |
| [H] | Every dependency is an optional pointer, and one is dereferenced unchecked | `PopContainer` |
| [M] | `ResourceManager` takes six nullable pointers and re-checks them at every use | `ResourceManager` |
| [M] | Optional dependencies with three different null policies per class | `FactionEffectsPool`, `ResearchManager`, `ResearchSelector`, `SocialEngineeringManager` |
| [M] | Split composition-root lifecycles (app / new game / load) | `Engine::Initialize_` |
| [M] | Keep `GameState` a session boundary, not an unbounded service bag | `GameState` |
| [M] | `main.cpp` has no top-level error handling | `main.cpp` |
| [M] | The `IUnitOrderWorld` overrides hand `GameState` its own RNG and tile-effects back | `GameState` |
| [M] | Conquest depends on a post-ctor nullable `GameDataContext` | `units/` conquest path |
| [M] | The executor is two-phase-initialised and its dependency is a nullable pointer | `DiplomaticActionExecutor` |
| [M] | `Military` leaks its owning container and swallows a null design | `Military` |
| [M] | `FirstContactResolver` depends on GameState's concrete ownership container | `FirstContactResolver` |
| [M] | `FactionIdentity` is a bypassed duplicate of the config it copies | `FactionIdentity` |
| [M] | Flavor RNG cannot be seeded, so base names are not reproducible | `FactionFlavor` |
| [L] | Hygiene blocks of the game-core and faction-economy sections | — |

### Problem statement

The dominant structural weakness of the codebase: objects become valid in stages after construction, and each class invents its own reaction to a null dependency (silently skip / return default / throw / dereference). `LoadGameData` mutates 22 default-null `unique_ptr`s; `GameState::AddFaction` re-wires a `Faction` in four steps; `BaseManager` takes ten nullable pointers with three policies. The single composition root supplies all of them today, so most can become references.

### Likely fix direction

- Constructor-injected references for everything the single composition root always supplies; keep pointers only where absence is a real, documented mode, and then handle it one way (throw) at construction.
- Make `LoadGameData` return a fully-built context (constructor or factory), not fill a default-constructed bag.
- One `Faction` construction path that ends valid; `AddFaction` performs registration, not construction.
- Split `Engine::Initialize_` into app-init / new-game / (future) load phases so `tests/GameFixtures.h` can share the same path.
- One seeded RNG source handed down (also serves package 1's WorldEvents item).

### Risks / invariants

This package touches nearly every constructor; sequence it before packages 6–10 so those do not re-litigate null policies. Do not merge with the god-facade split (`GameState`/`Faction` decomposition) unless analysis shows a cheap seam — that may deserve its own follow-up.

### Analysis output path

`docs/full-review-fix-prompts/04-composition-root-and-deps.md`

---

## Package 5 — Planetary Council: lifecycle, tally, outcomes, config schema

**Priority:** High. A shipped path bricks the council permanently.
**Theme:** The council state machine has explicit states, exits, and a config schema that cannot express something the runtime ignores.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | `m_activeProposalIds` conflates "law in force" with "already enacted" | `PlanetaryCouncil.cpp` |
| [H] | A pending vote has no exit other than unanimous participation | same |
| [M] | The AI stub is a free function, not a seam the council owns | `CouncilAiStub` |
| [M] | The "two most populous factions" governor rule is not enforced | `CastElectionVote` |
| [M] | `voteThreshold` is silently ignored for standard ballots | `TallyStandard_` |
| [M] | A governor veto on an election can never be overruled | `VetoUnanimouslyOverruled_` |
| [M] | Tallying is not separable or testable | tally helpers |
| [M] | The applier ignores per-effect targeting on instantaneous outcomes | `CouncilOutcomeApplier` |
| [M] | `ComputeVoteWeight` rebuilds the whole effect pool per member, per call | `PlanetaryCouncil.cpp` |
| [M] | A missing member silently turns a won election into a failure | `ResolveOutcome_` |
| [M] | `kind` and `election_outcome` are parsed independently, and mismatches half-apply | `CouncilProposalConfigParser` |
| [M] | `vote_threshold` is validated for every proposal but only honored by elections | same |
| [M] | `vote_threshold` is a `double` where the project has an exact rational type | `CouncilProposalConfig.h` |
| [M] | Interval defaults live in two places and a misspelled key falls back silently | `CouncilRulesConfigParser` |
| [M] | `required_proposals` documents an invariant the runtime does not hold | `CouncilProposalConfig.h` |
| [M] | Election ballot lists every member, not eligible governor candidates | `ui/council/CouncilVoteView` |
| [L] | Hygiene blocks of both council sections | — |

### Problem statement

The council's "state machine" is one `optional<PendingProposal_t>` plus a vector of ids meaning two different things, with no exit from a pending vote (AI members never vote in the shipped build, so a player-initiated vote bricks proposals forever). Election rules live in a function only the AI stub calls, while the UI decides candidates. The proposal schema accepts fields the runtime ignores.

### Likely fix direction

- Model the lifecycle explicitly (no pending / open ballot / resolved) with a defined absentee rule (missing ballot = abstain, as the tally already treats it).
- Introduce an `ICouncilVoter` seam the council drives for non-player members; stub becomes the first implementation.
- Extract tally as a pure function over (members, weights, ballots, threshold) and unit-test it.
- Split "in force" from "has passed" (`m_activeProposalIds` vs `m_passCounts`) and let `required_proposals` name which it wants.
- Reject config combinations the runtime cannot honor at parse time; parse `vote_threshold` as `Rational_t`.
- Honor `factionFilter` / `condition` on instantaneous outcomes, or reject them at load.

### Key files

`src/game/council/*`, `include/game/council/*`, `src/ui/council/*`, `config/council/*.json`, `tests/game/PlanetaryCouncilTests.cpp`, `docs/architecture/council-system.md`.

### Analysis output path

`docs/full-review-fix-prompts/05-council-lifecycle-and-schema.md`

---

## Package 6 — Base economy: worker assignment, resources, production, population composition

**Priority:** High. One player-visible display bug plus a pile of silent value defects.
**Theme:** Per-base queries answer per-base questions; the production queue and composition have stated contracts.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | `IsTileAssigned` answers "worked by anyone" to callers asking "worked by me" | `WorkerAssignmentManager` + two UI callers |
| [H] | `PopContainer` owns composition policy and rules services, not storage | `PopContainer` |
| [M] | `UserAssignBestAvailableWorker` ignores every failure and can strand a worker | `WorkerAssignmentManager` |
| [M] | Energy allocated to psych is a silent sink | `ResourceManager::ConsumePsych` |
| [M] | Golden-age inputs use `GetWorkerCount()`, which also counts drones and talents | population |
| [M] | The production queue has no contract for switching, surplus, or invalid input | production |
| [M] | `SetProduction` accepts any pointer and no layer validates the item | production |
| [M] | The `precedence` config key is parsed and ignored; the hardcoded order contradicts it | composition |
| [M] | `~BatchCompositionUpdate` runs work that can throw | composition |
| [M] | `RemovePop` is silent, arbitrary, and unobservable | `PopContainer` |
| [M] | Composition goes stale on every size change except growth | composition |
| [M] | Composition's `psych_output` is specialist psych only | composition |
| [M] | Production's minerals-per-row is the one game number still in code | production |
| [M] | `BaseSnapshot_t` carries an untyped production id that only round-trips for buildings | `BaseManager` |
| [L] | Hygiene blocks of the base-management (BaseManager / population / resources) sections | — |

### Problem statement

Overlapping base radii are normal, so the "is this tile worked" predicate answering world-wide makes the base screen show `0 0 0` on tiles that yield, and routes clicks to a no-op. Around it, the production queue accepts anything and defines nothing about switching or surplus, composition silently goes stale, and psych energy is charged but never spent.

### Likely fix direction

- Add an explicit "worked by this base" predicate (or make `GetWorkedTileYield` return `optional`); keep `IsTileAssigned` for availability.
- Give the production queue a stated contract (switch penalty/none, surplus carry, validation at the point of set) and validate items where they enter.
- Move composition policy out of `PopContainer`; recompute on every size change or make staleness impossible.
- Either consume psych or stop charging for it (a rule decision — leave a TODO rather than invent SMAC numbers).
- Move minerals-per-row to config.

### Key files

`src/game/faction/base/resources/*`, `src/game/faction/base/production/*`, `include/game/faction/base/population/PopContainer.h`, `src/game/population/*`, `src/ui/base/BaseWorkableAreaDisplay.cpp`, `src/ui/base/BaseView.cpp`, `config/` growth/composition files.

### Analysis output path

`docs/full-review-fix-prompts/06-base-economy-and-population.md`

---

## Package 7 — Units: model, orders, movement, path performance

**Priority:** Medium-high. Two [H]: an O(map) scan per step and a process-global.
**Theme:** Movement asks the world index, not the whole map; stacking state has one owner.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Stop scanning the whole map for hostiles on every step | movement rules |
| [H] | Stacking left the position index; the process-global is an incomplete prior fix | unit position index |
| [M] | Terraform completion ignores apply failure after energy was spent | `TerraformRules` |
| [M] | `NextStep` always runs a full Dijkstra | movement |
| [M] | Sea-former domain rules hardcode improvement ids | `TerraformRules` |
| [M] | `EmbarkInto` does not enforce carrier invariants | `TransportRules` |
| [M] | Conquest depends on a post-ctor nullable `GameDataContext` (shared with package 4) | conquest |
| [L] | Hygiene block of the units model/orders section | — |

### Likely fix direction

Use the existing position index for hostile queries; make stacking state a member of the owning world/session object rather than a process-global; cache or incrementally reuse path results for `NextStep`; roll terraform cost only after a successful apply (or refund); move improvement ids to config; enforce carrier capacity/domain in `EmbarkInto`.

### Key files

`src/game/units/MovementRules.cpp`, `UnitOrderExecutor.cpp`, `TerraformRules.cpp`, `TransportRules.cpp`, unit position index, `src/game/map/`, unit/movement tests.

### Analysis output path

`docs/full-review-fix-prompts/07-units-movement-and-orders.md`

---

## Package 8 — Units: combat, probes, conquest, morale

**Priority:** Medium-high. Wrong combat outcomes and silent probe failures.
**Theme:** Combat rolls happen before the state change they gate; probe/intercept context is accurate.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Roll `DisengageChance` before committing a withdrawal | `DisengageRules` |
| [H] | Keep the originating base on building intercept candidates | `InterceptRules` |
| [M] | Probe sabotage skips deploy-ledger notification and conflates facility vs random | `ProbeActionEffects` |
| [M] | Intercept condition context marks the wrong combat role | `InterceptRules` |
| [M] | Unit-subvert cost treats HQ-tile distance 0 as the no-HQ default | probe cost |
| [M] | Escape-pod design failures fail closed without error | conquest/escape pods |
| [M] | `risk_repeat` depends on a caller flag the executor never owns | probe executor |
| [L] | Hygiene block of the units combat section | — |

### Likely fix direction

Order the withdrawal so the roll gates the state change; carry the originating base through intercept candidate selection so building-sourced intercepts resolve with the right origin; give probe sabotage the same ledger notification as other probe actions and separate "sabotage this facility" from "sabotage a random one"; throw (or report) on escape-pod design failure instead of silently dropping; move `risk_repeat` ownership into the executor.

### Cross-package note

Probe sabotage destroying a Secret Project without a tombstone is fixed in package 9 (one tombstone rule for all destruction paths); this package only owns the ledger/targeting half.

### Analysis output path

`docs/full-review-fix-prompts/08-units-combat-and-probes.md`

---

## Package 9 — Buildings, secret projects, orbital census

**Priority:** Medium-high. Uniqueness is enforced only by the build menu.
**Theme:** Building/project invariants live at the mutation point; destruction is tombstoned everywhere.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Secret-project uniqueness is only checked when the build menu is generated | `BuildingManager::AddBuilding` |
| [M] | `IsCompleted` also answers true for projects that no longer exist | secret-project availability |
| [M] | Tombstone secret projects destroyed by ASAT | `OrbitalAttack` (+ probe sabotage, intercept) |
| [M] | Census count has two algorithms that can drift | `OrbitalCensus` |
| [M] | `category` is mandatory in the parser, undocumented, and never read | `BuildingConfigParser` |
| [M] | Typo'd or wrong-shaped keys are silently defaulted instead of rejected | same |
| [M] | Parse failures name neither the building nor the file | same |
| [M] | `BuildingConfig_t` has no default member initialisers | same |
| [M] | `BuildingRegistry` skips the validation extension point it inherits | `BuildingRegistry` |
| [M] | The config struct lives in the parser header, so `nlohmann/json.hpp` leaks everywhere | `BuildingConfigParser.h` |
| [L] | Hygiene blocks of the buildings and orbital sections | — |

### Likely fix direction

Enforce tech gate / `allowMultiple` / secret-project uniqueness in `AddBuilding` (the single mutation point); one tombstone helper called by every destruction path (raze, ASAT, probe sabotage, intercept); split `BuildingConfig.h` from the parser header; move whole-set checks into `BuildingRegistry::Validate_`; make the parser fail loud and name the file + building id.

### Analysis output path

`docs/full-review-fix-prompts/09-buildings-and-projects.md`

---

## Package 10 — Map runtime and world generation

**Priority:** Medium. Two [H] are generation-quality; one is an id-domain confusion.
**Theme:** Gameplay queries use config ids; generation invariants fail loud; the world is reproducible.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Query gameplay improvements with config ids, not sprite content ids | `Tile` / improvement queries |
| [H] | Reflow rivers after landmarks (elevation sculpt and terminators) | world generation |
| [H] | Make `CanBuildImprovement` enforce both directions of `excludes` | improvement rules |
| [M] | Stop exposing the owning tile vector as a mutable reference | `WorldMap` |
| [M] | Make `TerritoryMap::Rebuild` fail loud on size mismatch | `TerritoryMap` |
| [M] | Do not swallow missing Forest/KelpFarm config during spread | spread rules |
| [M] | Stop mutating fungus while probing spread eligibility | spread rules |
| [M] | Reject non-positive `WorldMap` dimensions in the constructor | `WorldMap` |
| [M] | Reject invalid map dimensions instead of returning an empty world | world gen |
| [M] | Surface Mount Planet sculpt knobs in landmark/sculptor config | world gen config |
| [M] | Do not silently skip empty landmark footprints | landmarks |
| [M] | Record the resolved seed when `seed == 0` | world gen |
| [L] | Hygiene blocks of both map sections | — |

### Likely fix direction

One id domain for gameplay improvement queries (sprite/content ids stay in rendering); run river flow after landmark sculpting; make `excludes` symmetric; throw on invalid dimensions and size mismatch; record the resolved seed into settings/state so a run is reproducible.

### Analysis output path

`docs/full-review-fix-prompts/10-map-and-worldgen.md`

---

## Package 11 — Config parsing, registries, and shared config/runtime libraries

**Priority:** Medium. Modder-facing silence; several "invent a value" paths.
**Theme:** Config either loads correctly or fails at load with a message naming the file, the id, and the key.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Reject empty cost formulas; do not floor eval failures into cost 1 | tech cost |
| [M] | Prerequisite validation misses cycles | `TechRegistry` |
| [M] | Missing tech `cost` silently becomes 0 | tech parser |
| [M] | Tech cost script load errors drop Lua diagnostics | tech cost |
| [M] | `GrowthConfigParser` silently defaults missing or mistyped keys | growth config |
| [M] | Composition parser allows empty formulas and records unused `precedence` | composition config |
| [M] | `PopTypeRegistry::Validate_` does not check internal pop-type references | pop types |
| [M] | Rating parser still hand-rolls file load and weak field access | `SocialRatingConfigParser` |
| [M] | Missing rating-table entry silently drops the axis | `SocialRatingResolver` |
| [M] | Delete unused `SocialScores` stub type | `SocialEffects.h` |
| [M] | `GameSettings` loads world-generation values from a user-editable file without validating them | `GameSettings` |
| [M] | Back-compat branch and struct/file layout drift in `GameSettings` | same |
| [M] | `k_GameCategoryCount` is a hand-maintained duplicate of the enum | `GameCategory.h` |
| [M] | Make `LuaRuntime::EvalInt` fail loudly and isolate variables | `lib/LuaRuntime` |
| [M] | Keep `Registry::Load` atomic across validation failure | `lib/Registry` |
| [M] | Reject wrong-typed arrays in `ParseStringArray` | `lib/config` |
| [M] | Harden `Rational_t::ScaledInt` against overflow | `lib/Rational` |
| [L] | Hygiene blocks of the research and social-engineering sections, plus the config/registry bullets of the shared-library section | — |

### Likely fix direction

Push every single-object parser through a shared object loader (the review notes at least five copies of open/parse/`is_object`); required keys via `.at()` with a message naming file+id+key; no invented balance numbers (effects package 5 set the precedent — mirror it); `Registry::Load` builds into a temporary and commits only on success; `magic_enum` for wire forms that differ only by case; delete the back-compat branch per guidelines.

### Analysis output path

`docs/full-review-fix-prompts/11-config-and-registries.md`

---

## Package 12 — Population rules and calculators

**Priority:** Medium. Rule disagreements between two entry points; potential hang.
**Theme:** One definition per rule (riot, specialist, obsolescence), tested directly.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | `ForceRiot` and `Update` disagree on what keeps a riot alive | riot calculator |
| [M] | `ResolveCurrentType` can hang on an obsolescence cycle | pop type resolution |
| [M] | Availability obsolescence is one level deep and order-tied | same |
| [M] | Composition calculator silently floors negative Lua results to zero | composition calculator |
| [M] | No direct tests for three of five calculators | tests |
| [M] | Make `IsSpecialist` exclude talents (and any golden-age contributor) | pop types |
| [M] | Inferring drone/talent identity from contribution thresholds couples role to magnitude | pop types |
| [L] | Hygiene blocks of both population sections | — |

### Likely fix direction

One riot predicate used by both paths; detect obsolescence cycles and throw; give pop types an explicit role/kind rather than inferring it from contribution magnitude; decide whether a negative composition result is a config error (throw) or a clamp (document it); add direct calculator tests.

### Analysis output path

`docs/full-review-fix-prompts/12-population-rules.md`

---

## Package 13 — Platform layer: event bus, graphics, input backends

**Priority:** Medium. One reentrancy UB; two backend abstraction leaks that block a headless mode.
**Theme:** Backends are substitutable; null objects are actually null; events survive handler mutation.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Snapshot handlers in `EventBus::Publish` (reentrancy UB) | `lib/EventBus` |
| [H] | `Display()` secretly owns the input pipeline and window-close policy | SFML graphics |
| [H] | Own pending key/mouse events inside the input backend | SFML input |
| [H] | `KeyFromSfKey` never returns `nullopt` — unmapped keys become `Unknown` events | SFML input |
| [M] | Font load failure silently disables all text drawing | SFML graphics |
| [M] | Presentation settings are hard-coded in the SFML TU | SFML graphics |
| [M] | `NullGraphics` fails texture ops that a null object should no-op successfully | `NullGraphics` |
| [M] | `LoadTexture` does not replace an existing id | graphics |
| [M] | `CaptureKey` discards modifiers that `CaptureKeyAsync` preserves | input |
| [M] | `*Async` methods are synchronous and not substitutable across backends | input |
| [M] | `NullInput` is a blocking console backend, not a null/headless object | `NullInput` |
| [M] | Hand-rolled key maps drift; `Key_tToString` omits F1–F12 | input |
| [L] | Hygiene blocks of the graphics and input sections, plus the `EventBus` bullets of the shared-library section | — |

### Likely fix direction

Copy/snapshot the handler list before dispatch (or use a stable structure with deferred removal); split window presentation from event pumping; move pending events into the input backend; make unmapped keys `nullopt` and drop them; make the Null backends genuine no-op/headless implementations so tests and CI can run without SFML; one key map next to the enum.

### Analysis output path

`docs/full-review-fix-prompts/13-platform-backends.md`

---

## Package 14 — UI: shared components and config-driven content

**Priority:** Medium. Five [H] are duplicated code or compiled-in content.
**Theme:** One list-selector/popup component; every id, glyph and key binding comes from config.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Extract the duplicated list-selector instead of letting copies diverge | `ui/base` selector popups |
| [H] | Stop cloning ProductionSelector for probe and supply popups | `ui/world` panels |
| [H] | Load terraform key→improvement bindings from config | `ui/world` order input |
| [H] | Retire or reuse dead `ComponentSlotDisplay` | `ui/unit-designer` |
| [H] | Stop growing a process-global god-object style registry | `ui/style` |
| [M] | Collapse duplicate identical style type pairs | `ui/style` |
| [M] | Do not store world elevation range in UI tile style | `ui/style` |
| [M] | Stop hardcoding the Forest improvement id in TileRenderer | `ui/TileRenderer` |
| [M] | Stop hardcoding improvement IDs in the map renderer | `ui/world` |
| [M] | Hard-coded pop display glyphs belong in config | `ui/base` |
| [M] | Hardcode supply-crawl resource choices in the popup | `ui/world` panels |
| [M] | Hardcoded category and rating axis tables drift from enums | `ui/social-engineering` |
| [M] | Long lists overflow the popup with no clip or scroll | selector popups |
| [M] | UnitStackPanel silently drops units that do not fit | `ui/world` panels |
| [M] | Click on a row can no-op when callback is empty or pointer is null | selector popups |
| [M] | Soft-skip empty callbacks instead of requiring a valid handler | `ui/world` panels |
| [M] | Narrow ViewFactory's header dependencies | `ViewFactory` |

### Likely fix direction

One scrollable, modal-aware list-selector component that the production / probe / supply / component / proposal popups all instantiate (this is also where package 2's modal rule and the unreachable outside-click dismiss get fixed once); style objects owned by the UI root rather than a process-global; every id/glyph/binding read from config with load-time validation.

### Analysis output path

`docs/full-review-fix-prompts/14-ui-shared-components.md`

---

## Package 15 — UI: view correctness, null-safety, and per-frame cost

**Priority:** Medium. Wrong or missing information on screen; per-frame recomputation of memoized data.
**Theme:** Views require what they need at construction, render from snapshots, and never silently swallow a missing dependency.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Do not return null views or push unchecked pointers | `ViewFactory` / `UIManager` |
| [H] | Do not open BaseView when a visible unit was selected on the tile | `ui/world/WorldView` |
| [H] | Finish `FormatFactionBonuses` — faction bonus line always shows "None" | `ui/social-engineering` |
| [H] | Ignore component `requiredTech` when listing and saving | `ui/unit-designer` |
| [M] | Selected design and draft state desync after edits | `ui/unit-designer` |
| [M] | Design list silently truncates overflow designs | same |
| [M] | Unknown slot `column` values fall through to left | same |
| [M] | Require non-null base/population at construction (use references) | `ui/base` |
| [M] | Drop the separate `Faction&`; use the base's owner | `ui/base` |
| [M] | Panels still force full live yield/production work every frame | `ui/base` |
| [M] | Player actions mutate managers with no command/event seam | `ui/base` |
| [M] | Recompute vote weights every frame with no revision cache | `ui/council` |
| [M] | Silent no-ops when council or player is missing | `ui/council` |
| [M] | `Resolve` from the cast callback is uncaught | `ui/council` |
| [M] | Do not silently drop proposal selection on gated failures | `ui/commlinks` |
| [M] | Wire or drop unused `playerCooldownYears` | `ui/commlinks` |
| [M] | Do not render a null `ResearchManager` as "None" | `ui/research` |
| [M] | Show the tech display name, not the config id | `ui/research` |
| [M] | Rebuild the entire view on every selection change | `ui/satellite` |
| [M] | Attack actions fail silently when prerequisites are missing | `ui/satellite` |
| [M] | Summary panel reallocates census data every frame | `ui/satellite` |
| [M] | Null player faction is swallowed | `ui/satellite` |
| [M] | Make `NewGameOnly` session-aware instead of permanently non-editable | `ui/settings` |
| [M] | Ignore non-left clicks in `HandleMouseClick` | `ui/settings` |
| [M] | Enforce descriptor kind/callback invariants before calling through | `ui/settings` |
| [M] | Stop treating every non-header/non-bool row as `ReadOnlyValue` | `ui/settings` |
| [M] | Bottom panel uses full-tech duration instead of remaining breakthrough turns | `ui/social-engineering` |
| [M] | Nullable deps deferred to Render; click path swallows null | `ui/social-engineering` |
| [M] | Stop culling closed views/elements inside Render | `UIManager` |
| [M] | Remove or rewire dead workable-area hit test | `ui/base` |
| [M] | Clarify HitTestWorldGrid contract | `ui/world` |
| [M] | Prefer fight-tile hit placement for the whole combat playback | `ui/world` |
| [M] | Resolve defender display name with the same unit Resolve attacked | `ui/world` |
| [L] | Hygiene blocks of all UI sections not claimed by packages 2 and 14 | — |

### Likely fix direction

References at construction for what a view cannot work without; a per-frame or revision-keyed snapshot for panels that currently call live getters per element (effects package 6 removed the worst tile-yield cost, but the UI still re-queries every frame); errors surfaced to the player rather than swallowed; unit-designer respects tech gating on both list and save.

### Risks

Large and shallow — split into two commits (per-view correctness, then per-frame snapshots) if the analysis says the diff would otherwise be unreviewable.

### Analysis output path

`docs/full-review-fix-prompts/15-ui-view-correctness.md`

---

## Package 16 — Architecture docs, mod seams, and hygiene sweep

**Priority:** Last, but the doc half blocks nothing and can land early.
**Theme:** The documented graph matches the live graph; mod seams have one real consumer.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | Refresh architecture docs to match the live graph | `docs/architecture/*` |
| [H] | Shape mod seams for a real consumer before more systems bypass them | hooks, `EventBridge`, `EventBus`, player actions |
| [M] | Align faction-subsystem docs with inter-faction ownership | `faction-system.md`, `high-level.md` |
| [L] | Architecture hygiene leftovers | `docs/architecture/*` |
| [L] | Residual naming / `_t` / `r`-prefix / dead-code bullets from every section not claimed above | codebase-wide |

### Problem statement

`faction-system.md` centers a `FactionManager` that does not exist, `high-level.md` lists `TileBonusRegistry` / `FactionFactory` / `SFMLUIManager`, and `turn-system.md` documents `ProcessTurn`. Separately, `Hook_t::callback` takes no context, parsers record `script_path` and never load it, `EventBridge` TODOs faction signals the docs claim are bridged, and player actions are direct UI→manager calls — so the moddability story has no consumer that would catch it breaking.

### Likely fix direction

- Doc refresh **after** packages 1–15 land the structural changes they will document (but the pure-drift corrections can go first).
- Pick one sample mod (a turn-stage hook plus one gameplay event) and make it work end-to-end; the seam design follows from that consumer rather than from speculation.
- Sweep the remaining `[L]` hygiene bullets per directory, one commit per section, no behaviour change.

### Analysis output path

`docs/full-review-fix-prompts/16-architecture-docs-and-hygiene.md`

---

## Package 17 — Faction services: treasury, social engineering, diplomacy, trade, visibility

**Priority:** Medium-high. Three [H]: an unforgeable-invariant gap, an all-or-nothing trade that is neither, and an O(map) recompute per unit event.
**Theme:** The class that owns a resource owns its rules; multi-item transactions are atomic; derived maps are recomputed incrementally.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|---------------|
| [H] | `EconomyManager` owns the treasury but offers no way to spend from it | `EconomyManager` + 7 caller sites |
| [H] | Trade items are validated one at a time and applied all at once | `DiplomaticActionExecutor` |
| [H] | Visibility rebuild is a whole-map, per-event recompute | `FactionVisibleMap::RebuildFromSources` |
| [M] | One global pending-proposal slot, silently overwritten | diplomacy |
| [M] | Unsized visible map silently means "sees everything" | `FactionVisibleMap` |
| [M] | `TradeKind` and its probe table duplicate the `TradeItem_t` variant three times | diplomacy / probes |
| [M] | Hardcoded default policy ids now hard-fail instead of silently failing | `SocialEngineeringManager` |
| [M] | `GetSocialRating` recollects and re-accumulates the whole rating map per query | same |
| [L] | Hygiene block of the faction military/diplomacy section | — |

### Problem statement

`AddEnergy(-cost)` is the spend API, so "the treasury never goes negative" lives in seven callers rather than in `EconomyManager`. A trade validates each item as it applies, so a multi-item deal can be half-executed. `RebuildFromSources` walks every tile of the world and re-resolves improvement sight radii (static config data) on every unit create/destroy. Starting policy ids are compiled-in string literals whose validation now hard-fails a modded game at faction construction, and `GetSocialRating` rebuilds a vector plus a map per axis per frame.

### Likely fix direction

- `SpendEnergy` / `CanAfford` (throwing) on `EconomyManager`; delete the duplicated affordability checks.
- Validate the whole trade first, then apply — or apply into a staged transaction that commits atomically.
- Cache the config-derived part of sight radius; rebuild visibility incrementally from the changed sources (a revision or dirty-set), and make an unsized visible map an error rather than "sees everything".
- Move starting policies to config (`default: true` per category or a `starting_policies` block), validated at load.
- Memoize the accumulated rating map against the revision `SocialEngineeringManager` already owns (mirrors what effects package 2 did for the pool).
- Collapse `TradeKind` + probe table onto the `TradeItem_t` variant (single source of truth).

### Key files

`src/game/faction/EconomyManager.cpp`, `DiplomaticActionExecutor.cpp`, `FactionVisibleMap.cpp`, `SocialEngineeringManager.cpp`, `src/game/units/ProbeAction*`, `config/social_policies.json`, faction/diplomacy/visibility tests.

### Cross-package note

Package 4 owns the constructor/null-policy half of these classes (two-phase-initialised executor, `Military` container leak, `FirstContactResolver` coupling); this package owns their behaviour. Sequence after package 4 or coordinate.

### Analysis output path

`docs/full-review-fix-prompts/17-faction-services.md`

---

## Cross-package dependency sketch

```text
[1 Turn pipeline] ──► [2 Modal/turn gating] ──► [14 UI shared components]
                                             └─► [15 UI view correctness]
[3 Lifetime/transfer] ──► [15] (view invalidation), [9] (tombstones)
[4 Composition root] ──► [6][7][8][9][10][11][17] (null policies settle first)
[5 Council] ── independent; consumes [2] for the vote-view gating half
[11 Config/registries] ──► [12 Population rules] (parser strictness first)
[13 Platform backends] ── independent
[16 Docs/mod seams/hygiene] ── last (doc-drift half may go first)
```

## Per-package analysis agent instructions (common)

Each analysis agent should:

1. Read this package section and every cited finding in `docs/full-code-review.md` in full.
2. Read the key source/header files, tests and config; **verify each claim at `path:line`** against the current tree. Effects packages 1–8 moved a lot of code — report findings that no longer reproduce instead of designing a fix for them.
3. Note interactions with other packages (blockers, merge conflicts, shared types).
4. Decide: confirm the review's fix direction, amend it, or split the package further — with rationale and rejected alternatives.
5. Write **only** the output file for that package, containing:
   - Short verified diagnosis (with `path:line` evidence)
   - Chosen design and rejected alternatives
   - Implementation plan
   - Requirement-based test plan (state explicitly where an existing test pinned the bug and must change)
   - A ready-to-paste **AI implementation prompt**: goals, constraints, key files, acceptance criteria, out-of-scope, what-not-to-do
6. Read-only otherwise. No builds during analysis; prefer not to run `./bd`.
