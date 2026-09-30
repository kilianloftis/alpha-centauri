# GameDataContext: follow the documented architecture, and stop testing shipping config

Based on `origin/main` at `d6d9743` (EcoDamage).

## Context

### The architecture we have

Recorded in `docs/architecture/high-level.md`, `map-system.md`, `docs/code-review-findings.md` §1.3 and the package 4 decisions:

- **Ruleset: `GameDataContext`.** Registries, configs and calculators. Loaded once by `LoadGameData`, complete when it returns, and not changed after loading.
- **Session: `GameState`.**
  - Game state plus the services that live as long as the map. Every stage, rule module and view receives it.
  - The generated world owns its elevation rules: `WorldMap` holds them and binds every `Tile` to them.
- **Aggregates: `Faction` → `BaseManager` → `Unit`.** `Faction` is the base factory. Leaf classes take narrow constructor references.
- **UI root: `ViewFactory`.**

### Where the code drifts

1. **Missing link: the session never got the ruleset.**
   - `GameState` takes six pieces of the context separately. `CreatePlanetaryCouncil` and `CreateWorldEvents` hand over more after construction.
   - Session-level code therefore has no source for definition data:
     - `UnitOrderExecutor` gets it through a late setter (`SetGameDataContext`), now for terraform as well as conquest.
     - The UI passes it down through its helpers.
     - New features reach through a faction instead. `FungalBloom_` borrows the first faction's context when no unit is involved.
2. **Aggregates hand the context out.** `Faction::GetDataContext()` has 22 call-site lines.
   - Session-level rules use it: triggered-effect dispatch, atrocity, vendetta, fungal bloom, the EcoDamage stage and commerce.
   - So do parts that a faction or base builds: `FactionEffectsPool`, `CommerceManager`, `UnitManager`, `BaseEcology`, composition inputs and base mood.
3. **Pass-through parameters.**
   - UI → `ProbeActionExecutor`, `IUnitOrderWorld` → `BaseConquestEffects`, and `TryFoundBase` → `CreateBase`.
   - Two of them are never used: `ResolvePostCombatBaseConquest` and `ApplyBaseAction_`.
   - `ApplyFungalBloom` takes the native registry as a parameter and then reads it again through the Planet faction.
4. **The ruleset is changed per session.** `Engine::StartNewGame_` writes the preset's elevation range into `GameDataContext::elevationRules`. Terraform reads that modified copy, while earthquakes read the world's own copy.
5. **Growing constructors.** `BaseManager` takes ten separate ruleset references, unpacked the same way at 4 sites. `BaseEcology` reaches through the faction rather than add more.
6. **Tests that break the new rule.**
   - 20 test files read `config/`, against `.devin/rules/testing.mdc` ("Do not test shipping config").
   - Some mix shipping and fixture registries in one session:
     - `ViewFixture` builds its `GameState` on fixture registries, but its hotkeys and `WorldView` on shipping ones.
     - The shipping-improvements options in `TerraformTests` and `FungalBloomTests` build the `GameState` on shipping improvements beside fixture components.

### Target rules

1. `Engine` owns the context and holds it `const`.
   - `GameState`, `Faction` and `ViewFactory` receive `const GameDataContext&` in their constructors.
   - Only `GameState::GetGameData()` gives it back out.
2. Classes below those owners take the pieces they use by constructor reference. A class that needs many pieces takes a named bundle of references.
3. Free rule functions and turn stages read `rGameState.GetGameData()` from the `GameState&` they already take. They never go through a faction, base or unit.
4. Only the loader, the validators and `MakeBaseRules` take `const GameDataContext&` as a parameter.
5. Rules for one world live on its `WorldMap`, not in the ruleset.
6. Tests never read `config/`.
   - A test that needs a value owns it in `tests/fixtures/`.
   - A test that only restates shipping data, or only checks that a shipping file loads, is removed.
   - Removing these is a requirement change under the testing rule, and the summary will list them.
7. The shipped config is checked by loading it at game startup, where `LoadGameData` throws on a bad file.

Game behaviour does not change.

## Changes

Work on a new branch from `main` (`d6d9743`), in 5 commits. Each builds and passes `./bd test` on its own.

### 1. Tests stop reading `config/`

New fixtures under `tests/fixtures/`. The shared files every `WorldFixture` loads keep their contents. Data only some tests need goes in new files that those tests load.

- `hotkeys.json`: the shipping bindings minus `SoilEnricher`, `MiningPlatform` and `TidalHarness`.
- `ui/style.json`: a copy of the shipping style.
- `tech_cost.lua`.
- A complete `LoadGameData` set, behind a `FixtureDataPaths()` helper in `TestHelpers.h`:
  - New files: `worldGen/presets.json`, `worldGen/decoration.json`, `worldGen/landmarks.json`, `pop_growth.json` and `base_conquest.json`.
  - `worldGen/presets.json` has the default preset, `islands`, at −4000 / 4000 like the shipping one. `TestMapRules()` gives every test map its elevation range from it.
  - `worldGen/landmarks.json` names fixture improvement ids. The escape pod in `base_conquest.json` names fixture component ids, such as `test_chassis` and `test_colony_pod`.
  - `techs_full.json`: `techs.json` plus every tech the other files in the set cite, so the set passes `ValidateRequiredTechReferences` and `ValidateEffectReferences`. `FixtureDataPaths()` points `techs` at it. The missing techs are at least:
    - `centauri_meditation`, `some_tech` and `gene_splicing` (buildings)
    - `monopole_magnets`, `advanced_military_algorithms`, `doctrine_air_power` and `ecological_engineering` (improvements, tile-yield rules)
    - `orbital_spaceflight`, `advanced_spaceflight`, `advanced_ecological_engineering` and `mind_machine_interface` (council proposals)
    - `gene_splicing` (probe actions)
  - `techs.json` stays as it is: every `WorldFixture` loads it, and seeded research picks draw from it.
  - `buildings_loadable.json`: `buildings.json` without `grantor_unknown`, whose grant of an unknown building `GrantExpansionTests` needs and the loader rejects. `FixtureDataPaths()` points `buildings` at it.
  - Fix any further validation failure in the set the same way, without changing what an existing test reads.
- `native_units_full.json`: the seven natives that `NativeUnitsTests`, `ArtifactLinkTests` and `ValidationTests` use, with the fields those tests check. They are `Mind_Worm`, `Isle_of_the_Deep`, `Sea_Lurk`, `Locusts_of_Chiron`, `Spore_Launcher`, `Fungal_Tower` and `Alien_Artifact`. The shared `native_units.json` keeps exactly one land and one sea lifeform, which `InstallNativeUnits` and the bloom tests rely on.
- `unit_components_specials.json`: the specials that `UnitStatsTests` and `ElevationChangeTests` use: `Colony_Pod`, `Probe_Team`, `Supply_Crawler`, `Terraformer`, `Transport`, and `Tectonic_Payload` with its `requires_chassis`.
- `difficulty/levels.json`, `difficulty/social_rating_effects.json` and `difficulty/pop_composition.json`: the difficulty levels, social-rating effects and pop-composition rules that `DifficultyTests` asserts against.
- A turn-stage file that lists every built-in stage id.
- `improvements_excludes.json` / `terrain_excludes.json` with the coexistence rules the terraform and bloom cases check. `WorldFixture` and `FactionFixture` take an optional occupant-file pair so a whole session can run on them.

Per file:
- **Delete** these cases:
  - `ConfigStrictnessTests`:
    - The four "shipped … load(s)" cases.
    - "The shipped social policies declare exactly one default per category". `SocialPolicyRegistry::Validate_` already enforces one default per category at load, and `SocialEngineeringManagerTests` covers a missing and a duplicate default.
  - `LandmarkGenerationTests`: "…production landmarks.json against production improvements".
  - `ExplosionTests`: "Shipping reactors set explosion radius…".
  - `FungalBloomTests`: "Shipping reactors set fungal bloom tiles…".
  - `ArtifactLinkTests`: "Network Node costs 20, upkeep 1…".
  - The shipping min/max checks inside `FungalBloomTests`' inverted-range case.
  - The shipping-components check at the end of `ElevationChangeTests`' "A special's requires_chassis list is enforced". `LoadGameData` runs `ValidateComponentChassisRequirements` at startup.
  - `CompositionRootTests`' stockpile-fallback check. A config with no tech-free stockpile is valid: minerals are wasted, and `StockpileConfigTests` requires `FindFallback` to return null in that case.
- **Switch to fixtures:**
  - `CompositionRootTests`: runs `LoadGameData(FixtureDataPaths())` and checks the context is complete. It no longer changes the working directory.
  - `ConfigStrictnessTests`: the long-colour case edits the fixture style.
  - `NativeUnitsTests`, `ArtifactLinkTests`, `ValidationTests`: `native_units_full.json`.
  - `UnitStatsTests`, `ElevationChangeTests`: `unit_components_specials.json`.
  - `TurnStageConfigTests`: the fixture stage list.
  - `EffectsCacheTests`, `ResearchSelectorTests`, `ProbeActionTests`: fixture `tech_cost.lua`.
  - `DifficultyTests`: the `difficulty/` files. `UseShippingDifficulty_` becomes `UseFixtureDifficulty_`.
  - `TestHelpers::LoadTestMapRules`: fixture presets.
  - `TerraformTests` and `FungalBloomTests`: replace their shipping-improvements options with the fixture occupant files.
- **UI:**
  - `HotkeyConfigTests`: fixture occupants plus fixture `hotkeys.json`. The "loads with Bombard and Farm both on F" case checks the fixture.
  - `TileRendererTests` and `ViewFixture` load the fixture style. `UiStyle` is shared across the whole test process, so every loader uses this one file.
  - `ViewFixture` builds `HotkeyConfig` from fixture hotkeys against fixture occupants. `WorldView` still reads occupants from the context, so until commit 2, `ViewFixture` keeps loading `improvementRegistry` and `terrainOperationRegistry` into it, now from the fixture files.
- Drop `AC_CONFIG_DIR` from `tests/CMakeLists.txt` once nothing uses it.

### 2. The session holds the ruleset

- `GameState`:
  - New constructor: `GameState(std::unique_ptr<WorldMap>, const GameDataContext& rGameData, GameSettings&, uint32_t rngSeed)`. It replaces the improvements, unit-components, morale, yield-rules, interaction-grids and world-rules parameters.
  - `m_rGameData` replaces `m_rMorale` and the copied `m_worldRules`.
  - Add `GetGameData()`.
  - Delete `GetMoraleCalculator()`. `ApplyDrainEnergy_` reads the calculator from `GetGameData()`.
  - `CreatePlanetaryCouncil()` and `CreateWorldEvents()` take no arguments.
- `ProbeActionExecutor`'s constructor takes `const ProbeActionsConfig_t&`.
- `Engine::StartNewGame_` uses the new constructor and the argument-free creators.
- Fixtures:
  - `WorldFixture`'s `improvements` and `unitComponents` become references into `dataContext`. They are filled in the member-initializer list the same way `LoadMapRules` already fills the context, so the ~230 references stay. Also load `probe_actions.json`.
  - `FactionFixture` stores `worldRules` in the context before building `pBindState`.
  - `MakeLandSession(rFixtures)` drops its improvements parameter.
  - All ~54 test `GameState` constructions become `(pMap, fixture.dataContext, settings, seed)`.
- Delete context reassignments that would free objects the session still references:
  - The `unitComponentRegistry` reloads in 5 tests.
  - `ProbeGame_`'s `probeActionsConfig`.
- Tests that need a different council or world-events config fill `fixtures.dataContext` before calling the creators. A fixture helper, `InstallCouncil(rData, proposalsFile)`, loads the fixture council. `WorldEventTests` assigns in place.
- `ViewFixture` uses the context's own registries.

### 3. Delete the pass-through parameters and the setter

- `UnitOrderExecutor`:
  - Delete `SetGameDataContext`, `m_pGameData` and `RequireGameData_`.
  - Delete the two `FindBaseAt` pre-checks that only narrowed that guard: the one in `ApplyLastDefenderConquest_` (its `m_pWorld` check stays) and the one in `ApplyArrivalEffects_`. `ResolvePostCombatBaseConquest` and `ResolveBaseEntryConquest` already return an empty result when no base is on the tile.
  - The constructor takes `const TerrainOperationRegistry&`, passed by `GameState` and by the nine test harnesses that build an executor (`MovementTests`, `PathfindingTests`, `BombardTests`, `CombatResolverTests`, `FuelTests`, `TransportTests`, `UnitPositionTests`, `ElevationChangeTests`, `MoraleCalculatorTests`).
  - `TryFoundBase` drops its context parameter.
  - Add `CanStartTerraformProject(const Unit&, const std::string& projectId, const GameState&) const`. `TryStartTerraform` uses it, and so does `WorldView`'s check for keys shared by two actions. A distinct name keeps it from hiding the free `CanStartTerraform` inside the executor.
- Terraform uses the world's rules:
  - `CanStartTerraform`, `TerraformEnergyCost` and `QuoteRaiseLowerEnergyCost` drop the `ElevationRulesConfig_t` parameter and read the acting tile's `MapRules()`.
  - `Engine` stops running `ApplyElevationRange` on the context, and holds it as `std::unique_ptr<const GameDataContext>`.
  - `WorldFixture` loads fixture `map_rules.json` into `dataContext.elevationRules` as parsed, the way `LoadGameData` does.
  - `WorldFixture` binds its map to `TestMapRules()`, which applies the preset range the way `WorldGenerator` does.
  - `WorldGenPipelineTests` keeps passing `dataContext.elevationRules` to `WorldGenerator`.
  - `TerraformTests`' raise/lower case reads the level sizes from the tile's `MapRules()`.
- `IUnitOrderWorld`, the `GameState` overrides and `BaseConquestEffects` drop the context parameter and read `GetGameData()`.
- `EnsureAdHocDesign` takes `const UnitComponentRegistry&`.
- `ProbeActionExecutor`'s methods, `ApplyProbeActionEffect` and `ApplyBaseAction_` drop the parameter.
- `Faction::CreateBase` and `CreateBaseFromSnapshot` drop it too.
- UI:
  - `ViewFactory` holds `const GameDataContext&`.
  - `WorldView` drops the context entirely.
  - `UnitOrderInputController::HandleMouse` and its 4 helpers drop `pDataContext`.
- Tests:
  - Delete the 9 `SetGameDataContext` calls.
  - Drop the argument at the ~38 `CreateBase*` sites (including `MakeSessionBase`), 13 `TryFoundBase` sites and 19 probe call sites.

### 4. Stop reaching through factions

- Session-level code reads `GetGameData()`:
  - `TriggeredEffectDispatch`: `Rebel_`, `GrantUnit_`, `FungalBloom_` and `ApplyUnitProducedTriggers`. `ApplyTechDiscoverEffects` stops reading the tech registry through the faction's research manager.
  - `CommitAtrocity`, `ApplyVendetta` and the EcoDamage stage.
  - `ApplyFungalBloom` drops its registry parameter. `PlaceLifeforms_` uses the single registry.
  - `CommerceCalculator`'s `TechDenominator_`.
- Add `BaseRules_t` (`include/game/faction/base/BaseRules.h`):
  - A struct of references to what a base uses: the ten current ruleset parameters, plus `DroneCalculator`, `EcoDamageCalculator` and `AtrocitiesConfig_t`.
  - Built by `MakeBaseRules(const GameDataContext&)`.
  - `BaseManager` copies the references it needs into its own members and its parts. It never keeps a reference to the bundle, because fixtures pass a temporary and bases move between factions.
  - `StockpileEnergyTests`' two helpers build their bundles from `MakeBaseRules`, putting the test's own stockpile registry in place of the context's. `MakeBaseWith_` also puts in its own building registry.
- The `BaseManager` constructor becomes `(Faction&, BaseId_t, std::string, Tile&, const BaseRules_t&, const SecretProjectAvailabilityCalculator*, TileEffectsContext&, initialPopulation, bMayOccupyWater)`:
  - It hands `PopulationManager` the drone calculator.
  - It hands `BaseEcology` the eco calculator and atrocity config.
  - `BuildCompositionInputs` and `ReadCompositionInputKey` take the drone calculator.
  - Callers:
    - `CreateBase*`
    - `MakeBase`, `MakeFactionBase`
    - `StockpileEnergyTests` ×2
    - `CommerceCalculatorTests` ×2 (the `ReadCompositionInputKey` calls)
- The `Faction` constructor passes narrow references:
  - `CommerceManager`: commerce config and Lua runtime.
  - `UnitManager`: interaction grids.
  - `FactionEffectsPool`: the difficulty and native-life-level configs, and the eco-damage config's effects list, like the other effect lists it already takes.
- `BaseMoodEffects` reads `rBase.GetPopulation().GetCompositionConfig()`.
- `EnsureNativeDesign` takes `const NativeUnitRegistry&`.
- Delete `Faction::GetDataContext()`.
- `DifficultyTests::UseFixtureDifficulty_` assigns into the existing config in place, so factions built earlier keep a valid reference.
- Null checks disappear where the member becomes a reference.
- Includes: files include only the headers they use. The forward declarations in `Engine.h` and `ViewFactory.h` become `struct GameDataContext;`.

### 5. Architecture docs

- `high-level.md`:
  - Layering and rules, including that rule functions and stages get the ruleset from `GameState::GetGameData()`.
  - `GameState` bullet.
  - `GameDataContext` bullet: `GameState` holds the context rather than borrowing the morale calculator, and `BaseRules_t` replaces the "`CreateBase` still unpacks" note.
  - Fixture sentence: tests build contexts from `tests/fixtures/` only, and the shipped config is checked when the game loads it at startup.
  - `GameState -.-> GameDataContext` edge in the diagram.
- `map-system.md`: terraform reads the world's rules.
- `unit-movement-system.md`: conquest and terraform data come from the session.
- `native-units-system.md`: new `EnsureNativeDesign` signature.
- `ui-system.md`: `WorldView` / `ViewFactory` dependencies.
- `ecology-system.md`: `BaseEcology`'s inputs.
- `docs/full-review-fix-packages.md`: package 7's deferred item, the shape of the `GameDataContext` dependency, is done. `GameState` holds the ruleset and `SetGameDataContext` is gone. The `GameState` / `Faction` god-facade split stays deferred.
- `docs/code-review-findings.md` §1.3: add a dated status note:
  - `Engine` owns the context, and `GameState`, `Faction` and `ViewFactory` hold it.
  - `Faction::CreateBase` no longer takes it.
  - `BaseManager` takes `BaseRules_t`.

## Verification

- After each commit, run `./bd all` (with `--no-sfml` if SFML is unavailable), then `./bd test`. Stop and report any failure.
- After each commit, also start the game from the repo root with `timeout 15 stdbuf -oL ./alpha-centauri`. Line buffering keeps the output `timeout` would otherwise lose. Startup runs `LoadGameData` on `config/`, which is the only check of the shipped config.
  - It must print `Starting game loop...` and still be running when `timeout` stops it (exit status 124).
  - `Fatal error: …` (exit status 1) or a crash is a failure. Stop and report it.
  - Without a display, build with `--no-sfml` first. That build runs the same startup on the null graphics backend.
- These suites exercise the changed paths:
  - Terraform, FungalBloom, Explosion, NativeUnits, ArtifactLink
  - Difficulty, CompositionRoot, ConfigStrictness, HotkeyConfig, WorldView
  - BaseConquest, Probe, FoundBase, Atrocity, EcoDamage, WorldEvent, CommerceCalculator, PlanetaryCouncil
  - WorldGenPipeline, StockpileEnergy, MoraleCalculator
- Grep checks:
  - After commit 1: `grep -rnE 'AC_CONFIG_DIR|\.\./\.\./config|"config"|current_path|LoadGameData\(\)' tests` returns nothing.
  - After commit 4: `grep -rn "GetDataContext\|SetGameDataContext\|GetMoraleCalculator" src include tests` returns nothing.
  - After commit 4: `grep -rnE 'GetGameState\(\)(->|\.)GetGameData' src include` returns nothing.
  - After commit 4: `grep -rnE "const GameDataContext& \w+[,)]" src include` (parameters, not local references) matches only `GameDataContext.h/.cpp`, the validators, `GameState`, `Faction`, `ViewFactory` and `MakeBaseRules`.
  - After commit 4: `grep -rn "ApplyElevationRange" src` matches only `WorldGenerator` and its parser.
- If a display is available, smoke-test the game: found a base, Farm with a Former, open a probe menu, end a turn.
