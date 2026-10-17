# Code organisation: one kind per file, one domain per directory

Based on `origin/main` at `96119e2` (Moisture generation update). Lands after
`docs/plans/current-game-state.md` and builds on its changes.

## Context

### What we have

The code already uses most of the right kinds, but only informally.

- Calculators are built from config and owned by `GameDataContext`. The rule for them is about where their numbers live (`calculator-config.mdc`), not what they may touch.
- Diplomacy is split into Rules (queries) and Effects (mutations), and package 19 extended the split to proposals. `BaseConquestRules` / `BaseConquestEffects` follow the same split. Nothing writes the split down as a rule for the rest of the code.
- Directories group code three different ways:
  - by domain: `research/`, `council/`, `atrocities/`
  - by owning aggregate: `faction/base/production/`
  - by kind: `population/calculators/`, `stages/`

### Where the code drifts

1. **Rules files that mutate.** Seven `*Rules.h` headers declare functions that change state or draw from the RNG:
   - `FuelRules`: `TryAssignAutoReturnToFuel`, `ProcessFuelAtTurnEnd`, `ProcessAllFuelAtTurnEnd`
   - `TransportRules`: `TryAttachToTransport`, `TryAutoAttachOnEntry`, `TryAutoAttachWhenMustLand`, `TryUnloadTransportInPlace`
   - `FoundBaseRules`: `ApplyStartingMinerals`
   - `TerraformRules`: `ApplyTerraformResult`
   - `ProbeRules`: `RollProbeAction` (RNG), `TryPromoteProbeMission`
   - `InterceptRules`: its only function, `TryInterceptAttack`
   - `VisibilityRules`: all three functions (`ApplyRemoveShroud`, `ApplyRemoveFog`, `ApplyVisibilityRules`). The visibility queries live in `UnitVisibility.h` instead.
2. **Files that mix kinds under a neutral name.**
   - `BuildingDestruction`, `MineralSupport`, `ScrapPayout`, `OrbitalAttack`, `ElevationChange` and `TerraformSpread` each hold a query beside the mutation it gates.
   - `TriggeredEffectDispatch.h` declares four queries beside the dispatchers.
   - `ActiveEffect.h` declares `ApplyModifyPopulation`, a mutation, among the resolve functions.
3. **The mutation suffix collides with the effects system.** "Effects" is the engine's word for config-driven modifiers. It appears in every config file, and in `BaseEffects_t`, `FactionEffectsPool` and `TriggeredEffect`.
   - `ProbeActionEffects` applies `probe_actions.json`'s `on_success_effects`.
   - `BaseConquestEffects` sits beside the `effects` list in `base_conquest.json`.
   - `BaseMoodEffects` and `CouncilEffects` are effects-system code with the mutation suffix.
   - A base mutation file cannot be called `BaseEffects.h`.
4. **Calculators that do more than evaluate formulas.**
   - `MoraleCalculator` reads units. Its `TryPromote` draws from the RNG and writes XP.
   - `CommerceCalculator` walks every faction, base, the diplomacy ledger and the atrocity ledger. Its only formula is one Lua expression.
   - `MoveCostCalculator` reads units and tiles and lives on `GameState`. `unit-movement-system.md` calls it "the single home of entry rules".
   - `SecretProjectAvailabilityCalculator` holds `const GameState&`.
   - `RiotCalculator` and `GoldenAgeCalculator` hold latched mood state and fire signals.
   - `GrowthCalculator` and `ProductionCostCalculator` are classes with deleted constructors and one static function each.
5. **Other names that claim the wrong kind.**
   - `CommerceManager` owns no save data. It memoises commerce lines.
   - `BaseRules_t` is the bundle of ruleset references `BaseManager` takes, not a rules file.
6. **A state owner that runs a command.** `PlanetaryCouncil` is the only state owner that writes outside itself. Its `Propose` and `Resolve` take `GameState&`.
   - `Propose` marks every member known in the diplomacy ledger.
   - `Resolve` applies passed proposals and the governor to factions through `CouncilOutcomeApplier`.
   - The council also holds the in-flight vote, the role `DiplomaticActionExecutor` plays for proposals.
7. **The RNG in a state owner.** `ResearchManager::PickRandomAvailableTech` draws from the session RNG for the triggered-effect tech grant.
8. **Directories.**
   - `faction/` (39 headers) holds 14 diplomacy headers, 6 visibility headers, commerce, the economy, and the research, social-engineering and unit managers.
   - `units/` (55) holds movement, combat, probes, base conquest, base founding, natives and terraforming.
   - `research/` and `social-engineering/` hold definitions while their managers sit in `faction/`.
   - Population code is split across `population/calculators/`, `population/pop-types/` and `faction/base/population/`.
   - `map/` holds 15 files that exist only for world generation, plus terraform mutations, the explosion and the fungal bloom.
   - Presentation data sits in `buildings/` (sprite sizes, overlay channels) and `map/` (occupant art).
   - `mind-control/` and `orbital/` hold one and two files.
9. `faction/Specialist.h` is an empty placeholder kept "for future specialist modifiers".
10. **Mechanic decisions sit inside the classes that apply them.** `docs/mechanic-decisions-review.md` lists them by domain, with file and line. Commits 7–13 move 75 of them. The heaviest concentrations:
    - `UnitOrderExecutor` decides several things itself:
      - what an attack costs, and who rolls for promotion
      - how a unit pays for a tile it cannot afford
      - how an airdrop lands
      - what a bombard offends and destroys
      - which units may found on water
    - `PlanetaryCouncil` holds vote weight, candidates, tallies and the veto as methods on the state owner.
    - `BaseManager`, `ProductionManager` and `ResourceManager` decide:
      - item cost, what can be built and what a halted base cannot do
      - inefficiency and the energy split
    - The `map/` mutations choose their own targets: the explosion, the fungal bloom, the elevation cascade, the spread and surface flips.
    - `CollectBombardTargets` decides both whether an artillery duel happens and what a strike hits.

    Three forecasts copy the logic of the mutation they predict instead of sharing it:
    - `WouldGrowThisTurn` / `WouldStarveThisTurn` beside `ApplyGrowth`
    - `BaseManager::WouldCompletionAbandonBase`, which re-derives growth
    - `WouldBeDestroyedWithoutRefuelThisTurn_` beside `ProcessFuelAtTurnEnd`

### Target rules

**Kinds.** Every file and class is one of these:

- **State owners** hold save data and keep it valid:
  - managers (`ProductionManager`), ledgers (`DiplomacyLedger`), entities (`Unit`, `Tile`), indexes, trackers and latches
  - change only the state they own and their own parts
  - never take or hold `GameState`
  - never draw from an RNG
- **Calculators** evaluate a subsystem's configured formulas:
  - `LoadGameData` builds them once and `GameDataContext` owns them
  - hold config, the Lua runtime and tables derived from config
  - methods are `const` and take plain values: numbers, ids, input structs, effect lists
  - never read the game: no `GameState`, no `CurrentGameState()`, no faction, base, unit or tile
  - never draw from an RNG and never mutate
- **Rules** answer a question about the game: legality, eligibility, targets, costs, derived values:
  - free functions, or a class that binds session-stable dependencies (`StepEvaluator`, `DisengageRules`)
  - inputs are `const`
  - never take `GameState`: they read it through `CurrentGameState()` (`current-game-state.md`)
  - never mutate and never draw from an RNG, because the UI, the AI and the pathfinder ask them speculatively
- **Mutations** carry out an outcome:
  - change state across owners, and take `GameState&` when they do
  - free functions, or a class that binds session-stable dependencies (`CombatResolver`)
- **Executors** run a multi-step command and hold its in-flight state (`UnitOrderExecutor`, `DiplomaticActionExecutor`, `ProbeActionExecutor`):
  - check rules, then call mutations
  - a command that is one mutation has no executor: its caller checks the rule and calls the mutation
- **Caches** memoise a view derived from state (`FactionEffectsPool`, `BaseEffectsCache`, `BaseEcology`). They own no save data.
- **Coordinators** are the turn stages.
- **Definitions** are `*Config_t`, `*ConfigParser`, `*Registry` and the validators.
- **The effects system** (`effects/`) follows the same split: resolving effects is rules, and dispatching triggered effects is mutations.

**Names.**

- `Manager`, `Ledger`, `Calculator`, `Rules`, `Mutations`, `Executor` and `Cache` name only files and classes of that kind. Every file holds one kind.
- A mechanic with its own files names them for the mechanic (`FuelRules`, `FuelMutations`).
- Small mechanics share their domain's file (`ProductionRules`, `ProductionMutations`).
- New logic goes into the matching file.

**Decisions.**

- A rules function makes every decision a player would call a mechanic: whether something may happen, which thing is chosen, how much is gained, lost or kept, and what follows. When the decision is a configured formula, a calculator makes it.
- State owners, mutations and executors call that function and apply what it returns. They hold no mechanic branches or thresholds of their own.
- A rule that decides several things at once returns them as one outcome struct, and the mutation applies it. `PredictModifyPopulationDelta` / `ApplyModifyPopulation` and `PlanScrapPayout` / `CreditScrapRefund` already work this way.
- Forecasts call the same rule as the turn, so a preview cannot disagree with what happens.
- A mutation draws from the RNG only within limits a rule set: a range, a chance, a candidate list.
- A consequence with no condition, choice or amount stays in the mutation that applies it. An example is a transferred base resetting its riot ladder. The change that adds a switch for one extracts its rule.

**Switches and signatures.**

- A switch between two versions of a mechanic is a field on its subsystem's config struct.
  - The subsystem's existing parser requires the field.
  - The rule that makes the decision reads the field and dispatches.
  - Each version is its own function in that Rules file.
- A rule reads a switch from a config struct it already takes, or else from `CurrentGameState().GetGameData()`. Adding a switch never changes its signature.
- Rules keep the smallest signature that answers the question. They take the objects the question is about, and read anything else from the current game state.

**Layout.**

- One directory under `include/game/` and `src/game/` per domain.
- The filename suffix says the kind.
- Domains are flat. There are no kind directories and no ownership nesting.

Game behaviour does not change.

## Changes

Commit straight to `main` once `current-game-state.md` has landed. The work is in 13 numbered steps, and each step lands as several smaller commits where it can: a rename, a split or a rule extraction per commit. Every commit builds, passes `./bd test` and starts the game (see Verification).

Commits 7–12 move each domain's decisions into rules, and commit 13 does the same for the council.
- The class that made each decision calls the rule and applies its result.
- Numbers and behaviour are unchanged.
- The ID in brackets after each item names its finding in `docs/mechanic-decisions-review.md`.

### Code style

Every new or changed line follows the six hard rules at the top of `.devin/rules/coding-guidelines.mdc`:
1. One call deep: name intermediate values.
2. Comments say why, in two lines or fewer, with no history and no references to plans or reviews.
3. No guards for cases that cannot happen.
4. No pass-through functions: when logic moves, its callers call the new home.
5. No flag parameters.
6. Nothing speculative.

Moved code keeps its body unless the move already touches a line. Each commit's diff is checked against these rules before it lands.

### 1. Record the kinds

- `docs/architecture/high-level.md`: new section "Code kinds" with the Kinds, Names, Decisions and Switches rules above.
- New `.devin/rules/code-kinds.mdc` in Cursor's rule format. `.cursor/` is a symlink to `.devin/`.
  - Frontmatter is `description` and `alwaysApply: true` only.
  - Body: the same rules in short form, and "a new mechanic switch goes in the Rules function that makes the decision".
- The other six rule files drop the `trigger: always_on` line, so every rule carries only Cursor's frontmatter.
- `CLAUDE.md` imports `@.devin/rules/code-kinds.mdc`.

### 2. Two small cleanups

- Delete `include/game/faction/Specialist.h` and `src/game/faction/Specialist.cpp`, and its line in `src/CMakeLists.txt`.
- `GameState` lists its world-effect sources once [EFF-1].
  - New private `AppendWorldSources_(const Faction* pExcluded, std::vector<ActiveEffect_t>&)` appends, in today's order:
    - every faction's `WorldGlobal` effects except `pExcluded`'s
    - the council's world effects
    - the world events
    - the standing world rules
  - `CollectSessionWorldEffects` calls it with no exclusion.
  - `CollectWorldExtras(rFor)` calls it excluding `rFor`, and adds the council's faction effects for `rFor` after the council's world effects, as today.

### 3. One directory per domain

This commit moves files only:

- No file is renamed and no code changes.
- The only edits are `#include` paths, source paths in `src/CMakeLists.txt` (targets unchanged) and paths in `docs/architecture/`.
- Each header and its source move together.
- At `96119e2` this is 160 headers and 128 sources, and 1,620 `#include` lines in 461 files across `src/`, `include/` and `tests/`.
- Test files stay where they are.

Moves, by destination (basenames; `.h` and `.cpp` alike):

- **`base/`**
  - from `faction/`: `RebelFactionPicker`
  - from `faction/base/`: `BaseEffectsCache`, `BaseManager`, `BaseMoodEffects`, `BaseRules`, `BaseTypes`, `HomeBaseIndex`
  - from `units/`: `BaseConquestConfig`, `BaseConquestConfigParser`, `BaseConquestEffects`, `BaseConquestRules`, `FoundBaseRules`
- **`buildings/`**
  - from `faction/base/`: `BuildingDestruction`
  - from `faction/base/buildings/`: `BuildingManager`
  - from `orbital/`: `OrbitalAttack`, `OrbitalCensus`
- **`combat/`** (new)
  - from `map/`: `Explosion`
  - from `units/`: `AttackRules`, `CombatResolver`, `CombatStrength`, `DisengageRules`, `InterceptRules`, `MoraleCalculator`, `MoraleConfig`, `MoraleConfigParser`, `PlanetPearls`, `ScrambleRules`, `StackCollateral`
- **`diplomacy/`** (new)
  - from `effects/`: `InfiltrationRules`
  - from `faction/`: `DiplomacyActions`, `DiplomacyConfig`, `DiplomacyConfigParser`, `DiplomacyLedger`, `DiplomaticActionExecutor`, `DiplomaticPermissionRules`, `DiplomaticProposalEffects`, `DiplomaticProposalRules`, `DiplomaticStatus`, `DiplomaticTransitionEffects`, `DiplomaticTransitionRules`, `FactionPair`, `FirstContactResolver`, `TradeItem`
- **`display/`** (new)
  - from `buildings/`: `BaseSpriteSizesConfig`, `BaseSpriteSizesConfigParser`, `MapOverlayChannelsConfig`, `MapOverlayChannelsConfigParser`
  - from `map/`: `OccupantArt`, `OccupantArtParser`
- **`ecology/`**
  - from `map/`: `FungalBloom`
- **`economy/`** (new)
  - from `faction/`: `CommerceCalculator`, `CommerceConfig`, `CommerceConfigParser`, `CommerceManager`, `EconomyManager`
  - from `faction/base/resources/`: `Inefficiency`, `MineralSupport`, `ResourceManager`, `WorkerAssignmentManager`
- **`effects/`**
  - from the root: `IEffectsProvider`, `IWorldEffectsSource`
- **`faction/`**
  - from the root: `Faction`
- **`movement/`** (new)
  - from `units/`: `AirdropRules`, `EvacuateTerritoryEffects`, `EvacuateTerritoryRules`, `FuelRules`, `MoveCostCalculator`, `MovementConstants`, `MovementRules`, `Pathfinder`, `StepEvaluator`, `TerritoryEntryEffects`, `TransportRules`
- **`natives/`** (new)
  - from the root: `NativeLifeLevelConfig`
  - from `units/`: `EnsureNativeDesign`, `NativeDesign`, `NativeUnitConfig`, `NativeUnitConfigParser`, `NativeUnitRegistry`
- **`population/`**
  - from `effects/`: `PoliceRulesConfigParser`
  - from `faction/base/population/`: `AssimilationTracker`, `AwayFromHomeDrones`, `CompositionInputs`, `GarrisonPolice`, `PopContainer`, `PopulationManager`
  - from `population/calculators/`: `DroneCalculator`, `GoldenAgeCalculator`, `GrowthCalculator`, `MoodLatch`, `PopCompositionCalculator`, `PopTypeAvailabilityCalculator`, `RiotCalculator`
  - from `population/pop-types/`: `GrowthConfigParser`, `Pop`, `PopCompositionConfigParser`, `PopTypeConfigParser`, `PopTypeRegistry`
- **`probes/`** (new)
  - from `mind-control/`: `MindControlLedger`
  - from `units/`: `ProbeActionConfig`, `ProbeActionConfigParser`, `ProbeActionEffects`, `ProbeActionExecutor`, `ProbeActionResult`, `ProbeRules`, `ProbeTarget`
- **`production/`** (new)
  - from the root: `ConstructableKind`, `IConstructable`
  - from `faction/base/production/`: `HurryProductionCalculator`, `ProductionApplyResult`, `ProductionCompletion`, `ProductionConfigParser`, `ProductionCostCalculator`, `ProductionManager`, `ProductionPauseGates`, `ScrapConfig`, `ScrapConfigParser`, `ScrapPayout`, `ScrapRefundCalculator`
  - from `stockpiles/`: `StockpileConfig`, `StockpileConfigParser`, `StockpileConversion`, `StockpileRegistry`
- **`research/`**
  - from `faction/`: `ResearchManager`, `ResearchSelector`
- **`social-engineering/`**
  - from `faction/`: `SocialEngineeringManager`
- **`stages/`**
  - from the root: `HookContext`, `TurnProcessor`, `TurnStageConfigParser`, `TurnStageFactory`, `TurnStageRegistrar`, `TurnStages`
- **`terraform/`** (new)
  - from `map/`: `ElevationChange`, `SurfaceOccupancy`, `TerraformSpread`
  - from `units/`: `TerraformRules`
- **`units/`**
  - from `faction/`: `Military`, `UnitManager`
- **`visibility/`** (new)
  - from the root: `VisibilityConfig`
  - from `faction/`: `FactionExploredMap`, `FactionRevealedUnits`, `FactionTileMemory`, `FactionVisibleMap`, `UnitVisibility`, `VisibilityRules`
- **`worldgen/`** (new)
  - from `map/`: `FbmNoise`, `FungusGeneration`, `LandmarkConfig`, `LandmarkConfigParser`, `LandmarkGeneration`, `MapGenerationConfig`, `MoistureGeneration`, `RockinessGeneration`, `TileBonusGeneration`, `WorldGenDecorationConfig`, `WorldGenDecorationConfigParser`, `WorldGenPresetConfig`, `WorldGenPresetConfigParser`, `WorldGenPresetRegistry`, `WorldGenerator`

Directories removed:

- `faction/base/` and its four subdirectories
- `population/calculators/` and `population/pop-types/`
- `mind-control/`, `orbital/` and `stockpiles/`

`atrocities/`, `council/` and `world-events/` keep their files. `RiverGeneration` stays in `map/`, because terrain changes call it at runtime.

`docs/architecture/high-level.md` gains a "Layout" section, one line per domain:

- (root): session and ruleset plumbing — `Engine`, `GameState`, `CurrentGameState`, `GameDataContext`, `GameSettings` and its setting structs, difficulty, player interactions, cross-domain validators, `GameCategory`
- `atrocities/`: atrocity records, sanctions and their consequences
- `base/`: the base aggregate, its ruleset bundle and effects cache, founding, conquest and rebellion
- `buildings/`: facility and secret-project definitions, a base's buildings, upkeep, destruction, orbital facilities
- `combat/`: attack legality, combat resolution, morale, interception, scramble, collateral, explosions
- `council/`: the Planetary Council, proposals and votes
- `diplomacy/`: statuses, the ledger, proposals and trade, transitions, permissions, first contact, infiltration
- `display/`: presentation data the ruleset loads — occupant art, base sprite sizes, overlay channels
- `ecology/`: eco-damage and fungal blooms
- `economy/`: base resource production, worker assignment, inefficiency, mineral support, energy allocation, commerce
- `effects/`: the effects system — effect and triggered-effect definitions, collection, resolution and dispatch, tile effects, interaction grids
- `faction/`: the faction aggregate, its definition, identity, flavour, AI profile, effects pool
- `map/`: the world map, tiles, territory, spatial indexes, occupants, rivers
- `movement/`: move costs, step legality, pathfinding, transport, airdrop, fuel, territory entry and evacuation
- `natives/`: native life designs, registry and native-life levels
- `population/`: pops and pop types, growth, drones, composition, police, mood latches, assimilation
- `probes/`: probe actions, targets, mind control
- `production/`: production, costs, completion, hurry, scrap, stockpiles, the constructable interface
- `research/`: techs, tech cost, research progress and selection
- `social-engineering/`: policies, social ratings, a faction's choices
- `stages/`: the turn processor, stage infrastructure and mod hooks, every turn stage
- `terraform/`: terraform projects, elevation change, terraform spread, surface flips
- `units/`: the unit aggregate, designs, components and slots, orders, the order executor
- `visibility/`: exploration, fog, tile memory, revealed units, visibility settings
- `world-events/`: scheduled world events
- `worldgen/`: world generation, presets, decoration

Historical records keep their paths:

- `docs/full-code-review.md`, `docs/code-review-findings.md`
- the fix-prompt directories
- `docs/plans/`, `.devin/plans/`, `memories/`

### 4. Names that claim the wrong kind

These are pure renames. Each rename changes the file, the type and every reference, tests included. Function names stay unless listed.

- The mutation suffix becomes `Mutations`:
  - `AtrocityEffects` → `AtrocityMutations`
  - `BaseConquestEffects` → `BaseConquestMutations`
  - `DiplomaticProposalEffects` → `DiplomaticProposalMutations`
  - `DiplomaticTransitionEffects` → `DiplomaticTransitionMutations`
  - `EvacuateTerritoryEffects` → `EvacuateTerritoryMutations`
  - `ProbeActionEffects` → `ProbeActionMutations`
  - `TerritoryEntryEffects` → `TerritoryEntryMutations`
- `visibility/VisibilityRules.h` → `VisibilityMutations.h`. Then `visibility/UnitVisibility.h` (`IsUnitVisibleTo`, `IsOwnerKnownTo`) → `VisibilityRules.h`.
- `combat/InterceptRules.h` → `InterceptMutations.h`.
- `movement/MoveCostCalculator` → `MoveCostRules`. This includes `MoveCostCalculator::Query` → `MoveCostRules::Query`.
- `buildings/SecretProjectAvailabilityCalculator` → `SecretProjectAvailabilityRules`.
- `population/RiotCalculator` → `RiotLatch`, and `population/GoldenAgeCalculator` → `GoldenAgeLatch`.
- `economy/CommerceManager` → `CommerceCache`. `Faction::GetCommerce` keeps its name.
- `base/BaseRules.h` → `BaseRuleset.h`: `BaseRules_t` → `BaseRuleset_t`, `MakeBaseRules` → `MakeBaseRuleset`.

### 5. Split files that mix rules and mutations

Moved functions keep their names and signatures.

- **`movement/`**
  - `FuelRules.h` keeps `IsRefuelSite` and `NeedsAutoReturnToFuel`.
  - New `FuelMutations.h`: `TryAssignAutoReturnToFuel`, `ProcessFuelAtTurnEnd`, `ProcessAllFuelAtTurnEnd`.
  - New `TransportMutations.h`: `TryAttachToTransport`, `TryAutoAttachOnEntry`, `TryAutoAttachWhenMustLand`, `TryUnloadTransportInPlace`. `TransportRules.h` keeps the rest.
- **`base/`**
  - New `BaseMutations.h`: `ApplyStartingMinerals`, from `FoundBaseRules.h`.
- **`terraform/`**
  - `TerraformRules.h` gains:
    - `FormerElevationChangeAllowed` and `FormerLowerFloorMeters`, from `ElevationChange.h`
    - `TerraformSpreadGrowthAttempts`, from `TerraformSpread.h`
  - New `TerraformMutations.h`: `ApplyTerraformResult`.
  - `ElevationChange.h` keeps its mutations and `RollLevelMeters`. `TerraformSpread.h` keeps its mutations.
- **`probes/`**
  - `RollProbeAction` and `TryPromoteProbeMission` move from `ProbeRules.h` to `ProbeActionMutations.h`.
  - `ProbeRollResult_t` stays in `ProbeRules.h`.
- **`buildings/`**
  - New `BuildingRules.h`: `CollectDestroyableFacilities`, `ListReadyOrbitalAttackers`, `OrbitalAttackerOption_t`.
  - New `BuildingMutations.h`: `DestroyBuildingAndNotify`, `DestroyRandomFacilities`, `TryAttackSatellite`, `OrbitalAttackResult_t`.
  - Delete `BuildingDestruction.h` and `OrbitalAttack.h`.
- **`economy/`**
  - New `EconomyRules.h`: `MineralSupportCost`.
  - New `EconomyMutations.h`: `ApplyMineralSupportAtBase`.
  - Delete `MineralSupport.h`.
- **`production/`**
  - New `ProductionRules.h`: `ScrapPayout_t`, `PlanScrapPayout`.
  - New `ProductionMutations.h`: `CreditScrapRefund`.
  - Delete `ScrapPayout.h`.
- **`effects/`**
  - New `TriggeredEffectRules.h`:
    - from `TriggeredEffectDispatch.h`: `TileHasVisitEffects`, `UnitHasHoldLink`, `HoldLinkHostName`, `UnitCanDetonate`
    - from `ActiveEffect.h`: `PredictModifyPopulationDelta`, `PredictTriggeredPopulationSize`, `PredictUnitProductionPopulationSize`
  - `ApplyModifyPopulation` leaves `ActiveEffect.h` for the anonymous namespace of `TriggeredEffectDispatch.cpp`, its only caller.
- **`research/`**
  - Delete `ResearchManager::PickRandomAvailableTech`. The `GrantTech` handler in `TriggeredEffectDispatch.cpp` draws from `GetAvailableTechs()` itself.

### 6. Calculators evaluate formulas only

- **`combat/MoraleCalculator`** keeps `GetConfig` and `DisplayName(int level, bool bNativeLifecycle)`.
  - Its unit-reading methods give way to value methods:
    - `int IntrinsicXp(bool bProbeTeam) const`
    - `int MoraleLevel(int xp, int liveBonus, CombatRole_t role) const`: clamped to the configured range, with the defender floor
    - `std::span<const EffectConfig_t> LevelEffects(int level) const`
    - `double PromotionChance(int xp, int attackStrength, int defenseStrength) const`: the seed formula through `ResolvePromotionChanceFromLevel`, 0 at the top level; it throws for an XP with no level, as `TryPromote` does today
  - New `combat/MoraleRules.h`:
    - `int BaseIntrinsicXp(const Unit&, const MoraleCalculator&)`
    - `int EffectiveMoraleLevel(const Unit&, const EffectContext_t&, const MoraleCalculator&)`
    - `std::span<const EffectConfig_t> EffectiveLevelEffects(const Unit&, const EffectContext_t&, const MoraleCalculator&)`
    - `const std::string& MoraleDisplayName(const Unit&, const MoraleCalculator&)`
    - the live-bonus helpers (`SumMoraleBonus_`, `HomeBaseHasCreche_`, `AdjustLiveBonus_`) move with them
  - New `combat/MoraleMutations.h`: `bool TryPromote(Unit& rSurvivor, int attackStrength, int defenseStrength, const MoraleCalculator&, std::mt19937&)`. It rolls against `PromotionChance` and sets XP.
- **`economy/CommerceCalculator`** evaluates the commerce formula only.
  - `int PairValue(const CommercePairInputs_t&) const`. `CommercePairInputs_t` holds `energyOurs`, `energyTheirs`, `commerceTech` and `techDenominator`; `pair_multiplier` comes from config.
  - `LoadGameData` builds it into the new `GameDataContext::commerceCalculator`, beside the other calculators.
  - New `economy/CommerceRules.h` takes everything else, with `CommercePartnerLine_t`:
    - `ComputeCommerceLines(const Faction&)`, from `ComputeAllLines`
    - `ComputeCommerceForFaction(const Faction&)`
    - `ComputeCommerceForBase(const BaseManager&)`
    - with them: the sanction checks, the tech denominator, base ranking, and the `CommerceRate` / `CommerceEnergyBonus` resolution
    - They read the game state and the calculator through `CurrentGameState()`.
  - `CommerceCache` takes only `(const Faction&)`, and no longer builds a calculator of its own.
- **`population/GrowthCalculator`** becomes `int ComputeNutrientsRequired(const GrowthConfig_t&, int baseSize, const BaseEffects_t&)` in new `population/GrowthRules.h`. Delete `GrowthCalculator.h`.
- **`production/ProductionCostCalculator`** becomes `int ComputeProductionCost(int baseCost, const BaseEffects_t&, int surchargePercent)` in `production/ProductionRules.h`. Delete `ProductionCostCalculator.h`.
- **`PopCompositionCalculator`** is borrowed as `const&` in `BaseRuleset_t` and `PopulationManager`. All its methods are already `const`.

### 7. Population decisions move into rules

- **Growth** (`population/GrowthRules.h`, beside `ComputeNutrientsRequired`) [POP-1]:
  - New `GrowthOutcome_t ResolveGrowth(const GrowthInputs_t&, const GrowthConfig_t&)`.
    - Inputs: the nutrient stockpile, net nutrients, the box at the current size and at the next size, and whether the base has room to grow. Intake per citizen comes from the config.
    - Outcome: whether the base grows or starves, and the stockpile afterwards.
  - It holds every branch of today's `ApplyGrowth`:
    - growth empties the box
    - a full box at the population limit keeps half
    - starvation empties the box
    - the deposit is floored at 0 and capped at the box
  - `PopulationManager::ApplyGrowth` and `CommitPendingGrowth` apply its outcome. `WouldGrowThisTurn` and `WouldStarveThisTurn` read it.
- **Pop loss** (new `population/PopulationRules.h`) [POP-2]:
  - `const Pop& SelectPopToRemove(const BaseManager&)`, from `PopulationManager::SelectDoomedPop_`.
  - It owns the pop's value as well: a specialist is worth econ + labs + psych, a worker its tile's yield, an unassigned pop 0.
  - The valuator lambda in `BaseManager`'s constructor and `PopulationManager`'s valuator parameter go.
- **Mood** (`population/PopulationRules.h`) [POP-3, POP-4]:
  - `bool RiotConditionMet(const RiotConditionInputs_t&, const PopCompositionConfig_t&)`. The inputs include the forced-riot turns remaining, so a riot is the natural condition or a forced one.
  - `bool GoldenAgeConditionMet(const GoldenAgeLatch::Inputs_t&, const PopCompositionConfig_t&)`.
  - `int ForcedRiotTurns(int remaining, int added)`, from `RiotLatch::ForceRiot`: the larger of the two.
  - The thresholds come from the config rather than the inputs. The latches keep only their state machine.
- **Assimilation** (`population/PopulationRules.h`) [POP-5]:
  - `AssimilationOnCapture_t AssimilationOnCapture(const AssimilationTracker&, FactionId_t previousOwner, FactionId_t newOwner, int peakDrones, int decayTurns)`.
  - It returns the occupier window, and the previous owner's new claim when it holds none. That covers a fresh window, a reversed window on recapture, and an earlier owner's claim kept across chained captures.
  - `AssimilationTracker::NotifyCaptured` stores what it returns.
- **Rebels** (`base/`) [POP-6]:
  - `RebelFactionPicker` becomes `RebelMutations` and keeps `PickRebelFactionAndTransfer`.
  - New `base/RebelRules.h`: `std::vector<RebelCandidate_t> RebelCandidateWeights(const BaseManager&)`. It excludes factions without bases, and each weight is the faction's join weight plus the distance bonus by mode.
  - The mutation rolls within the weights and transfers.

### 8. Production and building decisions move into rules

- **Retooling** (`production/ProductionRules.h`) [PRD-1]:
  - `int RetoolForfeit(int mineralStockpile, bool bHasTurnOriginal, bool bReturningToTurnOriginal, const ProductionConfig_t&, const BaseEffects_t&)` returns the minerals lost.
  - `ProductionManager::ApplyRetoolPenalty_` subtracts it.
- **Overflow** [PRD-2]: `int OverflowCarried(int leftover, const ProductionConfig_t&)`, from `ProductionManager::CompleteProduction_`. It reads `retoolPenaltyThreshold`, as today.
- **Cost** [PRD-3]:
  - `int ItemMineralCost(const BaseManager&, const IConstructable&)`, built on `ComputeProductionCost`: the prototype surcharge, and 0 for a stockpile.
  - `bool IsPrototype(const Faction&, const UnitDesign&)`, from `Military::IsPrototype`: a design is a prototype while any of its components has not been built.
  - `ProductionManager::GetMineralCost` and `Military::IsPrototype` are deleted, and their callers call these rules. `BaseManager::GetMineralCost` stays as the cost of the item the base is building.
- **What a halted base cannot do** [PRD-4, PRD-5]:
  - `bool CanBankProduction(const BaseManager&)`, read by `BaseManager::ApplyProduction`
  - `bool CanCompleteProduction(const BaseManager&)`, read by the completion gate in `ProductionManager` and `ProductionCompletion`
  - `bool CanHurry(const BaseManager&)`, read by `BaseManager::HurryProduction`, which keeps its throw as the precondition check
  - Each reads the `DisableProduction` flag.
- **Scrap** [PRD-6]: `bool CanScrapBuilding(const BuildingConfig_t&)`, beside `PlanScrapPayout`. Secret projects cannot be scrapped. `BaseManager::QuoteScrapBuilding_` calls it.
- **Constructable items** [PRD-7]: `std::vector<const IConstructable*> ConstructableItems(const BaseManager&)`, from `BaseManager::GetConstructable`, which is deleted. Its callers call the rule.
- **Abandon forecast** [PRD-8]: `bool CompletionAbandonsBase(const BaseManager&)`, from `BaseManager::WouldCompletionAbandonBase`. It predicts next turn's size through `ResolveGrowth`.
- **Building eligibility** (`buildings/BuildingRules.h`) [PRD-9]:
  - `bool MayHoldBuilding(const BuildingManager&, const BuildingConfig_t&)`: uniqueness and secret-project availability.
  - `BuildingManager::CanAddBuilding` and `GetBuildingsAvailableForConstruction` call it. The list adds the tech gate as today.
- **Scrap refunds** [PRD-10]: `std::optional<BaseId_t> ScrapRefundBase(const Faction&, const Tile&)`, from `Faction::ScrapDestinationFor_`.
- **Headquarters** (new `base/BaseRules.h`, a free name after commit 4) [PRD-11]: `bool GrantsHeadquarters(const Faction&)`. `Faction::CreateBase` calls it.

### 9. Economy, research and ecology decisions move into rules

- **Inefficiency** (`economy/EconomyRules.h`) [ECO-1]: `int EnergyAfterInefficiency(const BaseManager&, int energy)`, from `ResourceManager::ApplyInefficiency_`.
- **Allocation** (`economy/EconomyRules.h`) [ECO-2]:
  - `EnergySplit_t SplitEnergy(const Faction&, int energy)` holds the allocation split and its rounding: labs and psych are floored, and econ takes the remainder. It replaces `EconomyManager::CalculateEnergyForEcon`, `CalculateEnergyForLabs` and `CalculateEnergyForPsych`, and their callers update.
  - `EnergySplit_t AllocateBaseEnergy(const BaseManager&, int energy)`: inefficiency, the split, then the econ/labs/psych modifiers.
  - `ResourceManager`'s econ, labs and psych getters and `AllocateEnergy_` call `AllocateBaseEnergy`.
  - `AddAllocatedEnergy` keeps today's steps through `EnergyAfterInefficiency` and `SplitEnergy`, without the modifiers.
- **Worked yield** [ECO-3]: `TileResources_t WorkedYield(const BaseManager&)`, from `ResourceManager::ComputeWorked_`: worked tiles, supply crawlers and the base tile.
- **Minerals for production** [ECO-4]: `int MineralsForProduction(const BaseManager&)`, from `BaseManager::GetMineralsForProduction`, which is deleted. Its callers call the rule.
- **Unit support** [ECO-5]:
  - `int UnitSupportCost(const Unit&)`: native life on fungus is free. `MineralSupportCost` keeps the free-slot order and calls it.
  - `std::optional<UnitId_t> UnitToDisbandForSupport(const BaseManager&)`: the last charged home unit when the charges exceed the bank.
  - `ApplyMineralSupportAtBase` disbands what it returns until it returns nothing, then pays.
- **Work area** (`base/BaseRules.h`) [ECO-6]: `ForEachWorkableTile(const Tile& baseTile, WorldMapT&, Fn&&)` replaces `MapUtils`' `ForEachTileInWorkableArea`, and its callers update.
- **Working a tile** (`base/BaseRules.h`) [ECO-8]:
  - `bool MayWorkTile(const BaseManager&, const Tile&)`: the tile is in the base's work area and no one else works it. Work areas may overlap; a tile is worked by at most one base.
  - `WorkerAssignmentManager::AssignWorker` checks it before claiming.
  - `WorkedTileIndex::TryClaim` keeps exclusive claims as its invariant.
- **Research** (new `research/ResearchRules.h`):
  - [RES-1] `TechCostInputs(const Faction&, const TechConfig_t&)` returns the inputs `ResearchManager::ComputePointsNeeded_` gathers today. The formula stays in its calculator.
  - [RES-2] `int ResearchCarriedOver(int accumulated, int cost)`. `ResearchManager::DiscoverTech` keeps what it returns.
  - [RES-3] `std::vector<const TechConfig_t*> AvailableTechs(const Faction&)`. `ResearchManager::GetAvailableTechs` is deleted, and its callers call the rule.
- **Social engineering** (new `social-engineering/SocialEngineeringRules.h`) [RES-4]: `std::vector<const SocialPolicyConfig_t*> AvailablePolicies(const Faction&)`. `SocialEngineeringManager::GetAvailablePolicies` is deleted, and its callers call the rule.
- **Ecology** (new `ecology/EcologyRules.h`):
  - [ECL-1] `EcoDamageInputs(const BaseManager&)` returns the inputs `BaseEcology::CollectInputs_` gathers today. `BaseEcology` caches the result.
  - [ECL-2] `int AtrocityEcoMinerals(const AtrocityLedger&, FactionId_t)`, from `AtrocityLedger::EcoVirtualMinerals`.
  - [ECL-3] `int FungalPopChance(const BaseManager&)` and `std::vector<const Tile*> FungalPopTiles(const BaseManager&)`, from the `EcoDamage` stage, which rolls within them.

### 10. Combat decisions move into rules

- **Combat** (new `combat/CombatRules.h`) [CMB-1 – CMB-4]:
  - `ResolveCombatStrength` (`combat/CombatStrength.h`) takes the `CombatEngagement_t` in place of the defender stat, and picks Attack for a duel itself.
  - `CombatRoundOutcome_t ResolveCombatRound(const Unit& attacker, const Unit& defender, int attackRoll, int defenseRoll, CombatEngagement_t, int hpFloor)` returns the round winner and the damage each side takes, psi damage included. Equal rolls favour the defender, and a strike damages only the defender.
  - `bool PastDisengageThreshold(const Unit&, int startHp)`: the half-HP gate.
  - `CollateralOutcome_t PlanStackCollateral(const Unit& attacker, const Unit& defender, const WorldMap&)` returns:
    - the occupants the tile makes immune
    - the damage each other occupant takes
    - the wild natives destroyed outright
    - the non-combatants destroyed when no combatant remains
  - `ApplyStackCollateral` applies the collateral outcome.
  - `CombatResolver` keeps the round loop, the dice, the HP writes and the retreat moves.
- **Bombard targets** (`combat/AttackRules.h`) [CMB-5]:
  - `const Unit* ArtilleryDuelTarget(const Unit& attacker, const Tile&)`: the duel opponent when a hostile on the tile can bombard.
  - `std::vector<Unit*> BombardStrikeTargets(const Unit& attacker, const Tile&)`: empty when a hostile can bombard, legal or not.
  - Together they replace `CollectBombardTargets` and `BombardTargeting_t`.
- **Defender** [CMB-6]:
  - `Unit* SelectDefender(const Unit& attacker, const Tile&)` picks the visible hostile with the best chance of winning. Ties go to tile order.
  - New `double DefenderWinChance(const Unit& attacker, const Unit& defender)` in `combat/CombatRules.h` is a stub until combat odds are implemented. It returns the same value for every pair, so the first visible hostile defends, as today.
  - `ArtilleryDuelTarget` ranks bombard-capable hostiles the same way.
  - `FindAttackableHostileOnTile` keeps the legality checks and calls `SelectDefender`.
- **Promotion** (`combat/MoraleRules.h`) [CMB-7]: `std::optional<CombatSide_t> PromotionCandidate(const CombatResult_t&)`: the survivor of a kill. `UnitOrderExecutor::PromoteOnKill_` calls `TryPromote` on the answer.
- **Attack cost** (`combat/AttackRules.h`) [CMB-8]: `int AttackMoveFragments(const Unit&, AttackKind_t)`, with a new `enum class AttackKind_t { Attack, Bombard }`. One move point, or all remaining when the unit has `AttackingEndsTurn` or bombards. `UnitOrderExecutor::SpendAttackAction_` spends it.
- **Bombarding an empty tile** [CMB-9]: `std::vector<std::string> BombardImprovementTargets(const Unit&, const Tile&)`. It holds the non-base improvements when the tile is unoccupied, and nothing otherwise. It replaces `NonBaseImprovementIds`. `TryBombard` picks one.
- **Interception** (new `combat/InterceptRules.h`) [CMB-10]:
  - `std::vector<InterceptSource_t> CollectInterceptSources(const Unit& attacker, const Unit& defender)`, in today's order: faction-global, base, unit, world grantors, council, tile.
  - `InterceptOutcome_t ResolveIntercept(const InterceptSource_t&, int hitRoll, int destroyRoll)`: whether the attacker is destroyed, the cooldown is deployed and the source is destroyed.
  - `InterceptMutations::TryInterceptAttack` rolls, and applies the first ready source's outcome.
- **Satellite attack** (`buildings/BuildingRules.h`) [CMB-11]: `OrbitalStrikeOutcome_t ResolveOrbitalStrike(const OrbitalAttackEffect_t&, int hitRoll, int destroyRoll)`. `TryAttackSatellite` rolls and applies it.
- **Explosion** (new `combat/ExplosionRules.h`) [CMB-12]:
  - `ExplosionPlan_t PlanExplosion(const Tile& centre, int radius, const Unit* pSpared)` covers the tiles in the Chebyshev disk. For each tile it gives the units destroyed, whether a base is razed, the improvements removed and whether the tile drops a level.
  - `ApplyExplosion` applies the plan.
- **Wild natives** (new `natives/NativeRules.h`) [CMB-13]: `bool IsWildNative(const Unit&)` replaces the copies in `StackCollateral.cpp` and `PlanetPearls.cpp`.

### 11. Movement, terraform and map decisions move into rules

- **Paying for a tile** (`movement/MovementRules.h`) [MOV-1]:
  - `EntryPayment_t ResolveEntryPayment(const EntryTerms_t&, int available, int paidSoFar)`.
  - It returns whether the unit enters, the fragments it spends, and what it has paid afterwards.
  - `UnitOrderExecutor::SpendMovesAndEnter_` applies it.
- **Airdrop landing** (`movement/AirdropRules.h`) [MOV-2]:
  - `AirdropLanding_t AirdropLanding(const Unit&, const Tile&)` returns the HP after landing (never below 1) and whether the drop ends the turn (non-combat units only).
  - `UnitOrderExecutor::TryAirdrop` applies it.
- **Founding** (`base/FoundBaseRules.h`) [MOV-3]:
  - `bool CanFoundBase(const Unit&)`: the `FoundBase` flag, no founding on water except by a sea unit, and `CanFoundBaseAt` for the unit's tile.
  - `bool FoundedBaseMayOccupyWater(const Unit&)`.
  - `UnitOrderExecutor::TryFoundBase` calls both.
- **Fuel** (`movement/FuelRules.h`) [MOV-4]:
  - `EndOfTurnFuel_t ResolveEndOfTurnFuel(const Unit&, const WorldMap&)` decides whether the unit lands on a carrier, refuels, takes out-of-fuel damage or is destroyed. `OutOfFuelDamage` moves here with it.
  - `ProcessFuelAtTurnEnd` applies the outcome.
  - `NeedsAutoReturnToFuel` reads it, replacing `WouldBeDestroyedWithoutRefuelThisTurn_`.
- **Evacuation** (`movement/EvacuateTerritoryRules.h`) [MOV-5]:
  - `std::vector<const Unit*> UnitsToEvacuate(const Faction& guest, FactionId_t host)`: units on the host's territory whose owner the host knows.
  - `std::vector<const Unit*> UnitsSharingTilesWith(const Faction& guest, const Faction& host)`.
  - `EvacuateTerritoryMutations` takes their answers and keeps the group-then-individual placement.
- **Granted units** (new `units/UnitPlacementRules.h`) [MOV-7]:
  - `BaseManager* GrantAnchorBase(const Faction&, const BaseManager* pContextBase, const Tile* pTile)`, from `ResolveAnchorBase_`.
  - `std::vector<Tile*> PlacementTiles(const Tile& origin, const UnitDesign&, FactionId_t, int needed)`, from `PlaceableTilesNear_` and `DesignCanHoldTile_`.
- **Territory** (new `map/TerritoryRules.h`) [MOV-8]:
  - `std::vector<FactionId_t> ClaimTerritory(const WorldMap&, const std::vector<const BaseManager*>&)`. It returns the owner of each tile: the radius by surface, same-surface flood fill, the nearest base, and ties to the lower base id.
  - `TerritoryMap::Rebuild` stores the result.
- **Elevation** (`terraform/TerraformRules.h`) [TFM-1]:
  - `std::unordered_map<const Tile*, int> PlanElevationChange(const Tile& origin, int deltaMeters, int floorMeters, int ceilingMeters)` returns the new elevation of every tile the change moves. It runs the cascade on a working copy of elevations.
  - `ApplyElevationDelta` and `LowerTilesOneLevel` apply it, then reconcile surfaces and recompute rivers as today.
- **Earthquakes** (`terraform/TerraformRules.h`) [TFM-7]: `int EarthquakeDeltaMeters(std::span<const int> levelRollsMeters, const ElevationRulesConfig_t&)`. It sums the level rolls, capped at the full elevation span. `ApplyEarthquake` rolls once per level and applies the result.
- **Spread** (`terraform/TerraformRules.h`) [TFM-2]:
  - `const Tile* PickSpreadTarget(const Tile& origin)`, beside `TerraformSpreadGrowthAttempts`.
  - It takes the eligibility checks (altitude gate, rocky land, base or existing copy) and the neighbour score.
  - `TrySpreadTerraformFromTile` places on its answer.
- **Surface flips** (`terraform/TerraformRules.h`) [TFM-4]: `SurfaceFlipConsequences_t SurfaceFlipConsequences(const Tile&, bool bNowWater)` returns the improvements removed and whether the base is razed. `ReconcileSurfaceFlips` applies it.
- **Fungal bloom** (`ecology/EcologyRules.h`) [TFM-6]:
  - `std::vector<Tile*> BloomCandidateTiles(const Tile& origin)`: the origin unless it is a base, then radius-1 neighbours that are neither base nor fungus.
  - `std::vector<const NativeUnitConfig_t*> EligibleLifeforms(const Tile&)`.
  - `ApplyFungalBloom` shuffles and rolls within them.

### 12. Diplomacy, conquest and probe decisions move into rules

- **Hostile acts** (`diplomacy/DiplomaticTransitionRules.h`):
  - [DIP-1] `std::optional<VendettaKind_t> VendettaFromHostileAct(const HostileAct_t&)`. `ApplyHostileAct` declares the Vendetta it returns.
  - [DIP-2] `bool VendettaWithdrawsTerritory(VendettaKind_t)`: a declaration withdraws, a sneak attack does not. `EnterVendetta_` reads it.
  - [DIP-3] `bool HonoredObligationCascades()`: true under `SeparateDeclaration`. `HonorDefensiveObligation` declares on true and enters quietly on false.
  - [DIP-4] `std::vector<HostileAct_t> BombardHostileActs(const Unit& attacker, const Tile& target)`, from `UnitOrderExecutor::BombardHostileActs_`, built on commit 10's bombard target functions.
- **Atrocities** (`atrocities/AtrocityRules.h`):
  - [DIP-5] `std::vector<FactionId_t> UniversalVendettaJoiners(FactionId_t perpetrator, std::optional<FactionId_t> victim)`, from `ApplyUniversalVendetta_`.
  - [DIP-6] `AtrocityOutcome_t ResolveAtrocity(const Faction& perpetrator, const Faction* pVictim, AtrocitySeverityId_t authored)`:
    - It returns the excuse, whether the atrocity counts, the effective severity, the sanction years, and whether universal Vendetta and expulsion follow.
    - `CommitAtrocity` records it, applies the hostile act, extends the sanction and applies the penalties it names.
- **Base conquest** (`base/BaseConquestRules.h`) [BCQ-1 – BCQ-4]:
  - `std::optional<ConquestKind_t> ConquestOnEntry(const Unit&, const BaseManager&)` and `std::optional<ConquestKind_t> ConquestOnLastDefenderKill(const Unit&, const BaseManager&)`: capture, native raid or last-defender casualty.
  - `CaptureOutcome_t PlanCapture(const Unit& capturer, const BaseManager&)` returns:
    - the range of facilities destroyed
    - the capture pop loss, or for a cross-species capture the clash split: pops removed, and pops that flee as escape pods
    - whether the capturer is repaired
  - `NativeRaidPlan_t PlanNativeRaid(const BaseManager&)` returns the facility candidates, the facility chance (0, 50 or 100) and whether a pop can be lost.
  - `BaseConquestMutations` draws within these, spawns the pods and transfers.
- **Probes** (`probes/ProbeRules.h`):
  - [PRB-1] `int MissionRisk(const ProbeActionConfig_t&, const ProbeTarget_t&, const BuildingId_t& facilityId, bool bRepeatAtBase)`, from `ProbeActionExecutor::ResolveMissionRisk_`.
  - [PRB-2] `int StealEnergyAmount(const BaseManager&, const Faction& target, int morale)`, from `ProbeActionMutations`.
  - [PRB-3] `std::vector<TechId> StealableTechs(const Faction& actor, const Faction& target)`, from `PickStealableTech_`. The mutation picks one.
  - [PRB-4] `ProbeChance_t ProbeChancesFor(const Unit&, const ProbeTarget_t&, int risk)`, from `ProbeActionExecutor::FillProbeChances_`. It chooses the failure and success scales and calls `ComputeProbeChances`.

### 13. The Planetary Council keeps only its own state

- New `council/CouncilRules.h`:
  - the two queries that `current-game-state.md` already stopped passing `GameState`:
    - `bool HasCommlinksToAllMembers(const PlanetaryCouncil&, const Faction&)`
    - `int YearsUntilCanPropose(const PlanetaryCouncil&, const Faction&)`, with `ProposeCooldownYears`
  - [CNC-1] `int VoteWeight(const PlanetaryCouncil&, const Faction&, CouncilVoteWeight_t)`, from `ComputeVoteWeight`
  - [CNC-2] `std::vector<Faction*> GovernorCandidates(const PlanetaryCouncil&)` and `std::vector<Faction*> EligibleCandidates(const PlanetaryCouncil&, const CouncilProposalConfig_t&)`
  - [CNC-3] `bool ProposalPasses(const PlanetaryCouncil&, const PendingProposal_t&)` and `Faction* ElectionWinner(const PlanetaryCouncil&, const PendingProposal_t&)`, from `TallyStandard_` and `TallyElection_`
  - [CNC-4] `bool CanVeto(const PlanetaryCouncil&, const Faction&, const PendingProposal_t&)` and `bool VetoOverruled(const PlanetaryCouncil&, const PendingProposal_t&)`, from `VetoPending` and `VetoUnanimouslyOverruled_`
  - [CNC-5] `bool CanPropose(const PlanetaryCouncil&, const Faction&, const std::string& proposalId)`
- New `council/CouncilMutations.h` replaces `CouncilOutcomeApplier`:
  - `ApplyPassedCouncilEffects(GameState&, const std::vector<Faction*>& members, const CouncilProposalConfig_t&)`
  - `ApplyCouncilGovernor(GameState&, const std::vector<Faction*>& members, Faction& governor, const CouncilRulesConfig_t&)`
- New `council/CouncilVoteExecutor`. `GameState` owns it and creates it in `CreatePlanetaryCouncil`, and `GameState::GetCouncilVotes()` returns it.
  - It takes from `PlanetaryCouncil`:
    - the pending proposal (`PendingProposal_t`, `GetPending`) and its eligibility snapshot
    - `Propose`, `CastVote`, `CastElectionVote`, `VetoPending`, `AllMembersVoted` and `Resolve`
    - `OnProposalOpened` and `OnResolved`
  - `Propose(GameState&, Faction&, id)` and `Resolve(GameState&)` run today's steps in today's order:
    - propose-time checks through `CouncilRules`
    - `SetKnown` on the ledger
    - the tally and the veto through `CouncilRules`
    - outcome mutations before `OnResolved`
    - clearing the pending slot when an observer throws
- `PlanetaryCouncil` keeps its own state and the queries over it.
  - State: membership, governor, the supreme-leader stub, active proposals, pass counts, last-proposed years, council effects and its revision.
  - Queries: `IsCouncilMember`, `LastProposedYear`, `IsActive`, `HasPassed`, `HasActiveRuleFlag`, `CollectWorldEffects`, `CollectFactionEffects`.
  - New transitions the executor calls:
    - `RecordProposal(FactionId_t, int missionYear)`
    - `ApplyPassed(const CouncilProposalConfig_t&)`: repeals, pass count, activation
    - `InstallGovernor(Faction&)`
    - `SetSupremeLeaderVictory(Faction&)`
  - No method takes `GameState`.
  - Pending-vote changes no longer bump the council's revision. `CouncilVoteWeightCache` reads it, and vote weights do not depend on the pending vote.
- These callers move to the executor or to `CouncilRules`:
  - `Engine` (AI stub wiring) and `CouncilAiStub`
  - `CommlinksView`, `CouncilVoteView`, `CouncilFactionVotesPanel` and `CouncilProposalInfoPanel`
  - `CouncilVoteWeightCache`
  - the council tests
- Update `docs/architecture/council-system.md`.

## Tests

- Tests follow every move and rename.
- No test is deleted, and none changes what it asserts.
- Tests of a split function keep calling it under its new header.
- `MoraleCalculatorTests` exercises the morale value methods, `MoraleRules` and `MoraleMutations`.
- The commerce tests reach the formula through `CommerceRules`.
- The council tests drive votes through `CouncilVoteExecutor`.
- Each rule extracted in commits 7–13 gets direct tests of the requirement it encodes. They are written from `docs/game-rules-decisions.md`, `docs/mechanic-decisions-review.md` and the existing behavioural tests, not from the implementation.
  - Growth: at the population limit a full box keeps half.
  - Combat: equal rolls favour the defender.
  - Collateral: a tile with a MaxClamp to 0 makes its occupants immune.
  - Artillery: a hostile that can bombard makes the attack a duel, and leaves no strike targets.
  - Forecasts: `WouldGrowThisTurn`, `CompletionAbandonsBase` and `NeedsAutoReturnToFuel` agree with the turn on the same inputs.
- The rules that keep today's order where order is the mechanic get a test that pins it: `SelectDefender`, `CollectInterceptSources` and `ElectionWinner`'s tie-break. A later change to that order is then a visible requirement change.
- The behavioural tests still pass through the appliers.

## Verification

- After each commit:
  - `./bd all` (with `--no-sfml` when SFML is unavailable), then `./bd test`.
  - Start the game from the repo root with `timeout 15 stdbuf -oL ./alpha-centauri`. It must print `Starting game loop...` and still be running when `timeout` stops it (exit status 124). Startup runs `LoadGameData` on `config/`, the only check of the shipped config.
- Stop and report any failure.
