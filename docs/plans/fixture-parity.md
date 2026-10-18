# Fixture parity: every test fixture is one game session

Based on `main` at `8dbb2a7`. Lands after `docs/plans/code-organisation.md`.

## Context

### What we have

- `WorldFixture` owns a `WorldMap` and a `TileEffectsContext` of its own. No `GameState` owns them, and the tile effects are not bound to any world effects.
- `BaseFixture` has no game state, so `CurrentGameState()` throws in its tests.
  - Its `MakeBase` bases are standalone. Their owner faction does not list them, so their buildings never reach their base effects, which come from the faction's pool.
- `FactionFixture` lends `pBindState`, a `GameState` over a 2×1 stand-in map.
  - Its factions are not in that state, so `Factions()` is empty.
  - It copies part of `GameState::AttachToSession_` by hand: forgetting destroyed units, rebuilding territory, tombstoning destroyed secret projects, applying unit-produced triggers.
  - It does not copy the rest: spawn services, tech-discovery effects, first contact, production `on_complete` effects, mood notices.
- Fixture ids come from fixture counters (`nextFactionId`, `nextBaseId`, `nextUnitId`), not from the session's allocators. `BaseFixture`'s owner faction and `FactionFixture`'s first faction both have id 1.
- 29 test files, `DiplomacyFixture` and `ViewFixture` build a `GameState` of their own over the fixture's data, because the fixture has none. Three of the files do it through `MakeLandSession`.
  - Ten of the files give that state a `GameSettings` of its own while their factions read the fixture's.
- `GameState::AddFaction` does not check that a faction was built against its map, because fixture factions are not.
- Because of these gaps, many rules take the map, the tile effects and other context as parameters. `docs/architecture/high-level.md` says rules take the objects the question is about and read anything else from the current game state.

### Target

1. Every fixture is one game session, built the way `Engine` builds one. `WorldFixture`'s `GameState` owns the fixture map and its tile effects, and its scope is current for the fixture's lifetime.
2. Fixture factions, bases and units belong to that session. They join it through `AddFaction`, and their ids come from its allocators.
3. A test builds a `GameState` of its own only to compare two sessions.
4. Free rule functions stop taking the map, the tile effects and the other context listed below. They read it from the current game state.
   - `ClaimTerritory` keeps its map, because the map is what it claims.
   - Rule classes (`MoveCostRules`, `DisengageRules`) keep the dependencies their constructors take.
   - Rules keep the ruleset parameters `code-organisation.md` gave them, such as `ResolveGrowth`'s `GrowthConfig_t` and the morale rules' `MoraleCalculator`.

Game behaviour does not change. Fixtures now behave like the game where they did not, so some test outcomes change. Each step lists the changes to expect.

## Changes

Commit straight to `main`. Each numbered step may land as several smaller commits. Every commit builds, passes `./bd test` and starts the game (see Verification).

### Code style

Every new or changed line follows the six hard rules at the top of `.devin/rules/coding-guidelines.mdc`:
1. One call deep: name intermediate values.
2. Comments say why, in two lines or fewer, with no history and no references to plans or reviews.
3. No guards for cases that cannot happen.
4. No pass-through functions: when logic moves, its callers call the new home.
5. No flag parameters.
6. Nothing speculative.

### 1. The fixture's game state owns its map

- New `actest::SessionInputs` in `tests/GameFixtures.h`. It holds everything the session's factions and units reference, so that it outlives the session:
  - `dataContext`, `settings`, and the `improvements` / `unitComponents` references
  - `factionDefinition`, `ownerDefinition`, `extraDefinitions` and `designs`, moved from `BaseFixture` and `FactionFixture`
  - Its constructor loads what `WorldFixture` loads today. It takes the occupant files and the world rules, so `dataContext.worldRules` is set before any `GameState` exists.
- `WorldFixture` derives from `SessionInputs`. Its members:
  - `ac::GameState state`, over a new `WorldMap(width, height, TestMapRules())`, `dataContext`, `settings` and `k_TestRngSeed`
  - `ac::GameStateScope scope`, over `state`
  - `ac::WorldMap& map` and `ac::TileEffectsContext& ctx`, the session's own. The fixture's separate `TileEffectsContext` is deleted, and `*fixture.ctx` becomes `fixture.ctx`.
- `FactionFixture`:
  - `pBindMap`, `pBindState` and `pBindScope` are deleted. Their users use `state`.
  - Its `OnUnitMoved` connection is deleted, since `GameState` connects the same visibility rebuild.
  - Its `worldRules` parameter is passed through to `SessionInputs`.
- `ViewFixture` builds no `GameState`. Its views read the `WorldFixture` session.
- `BombardTests` drops its `BindWorldEffects` call.
- "Building production without spawn services completes without on_complete effects" builds its faction and base outside the session. A `Faction` built against the fixture's map and never added stands in for `MakeBase`. The test asserts that the session does not list it, instead of asserting that no game state is current.
- Expected outcome changes:
  - Fixture tiles see the session's world-global tile effects.
  - `BaseFixture` and `WorldFixture` tests have a current game state. `FactionEffectsPool`, `CommerceCache` and `Faction::EnsureComposedEffects_` find a session where they found none.

### 2. Tests use the fixture's session

- Each test session struct loses its `GameState`, `GameStateScope` and `GameSettings` members, and uses its fixture's `state` and `settings`.
  - Its map setup acts on the fixture's map before any faction is added: the size through the fixture constructor, then any land or water fill.
  - Its factions are added to the fixture's `state`.
  - Tests that set the struct's settings set the fixture's.
- `DiplomacyFixture` does the same.
- `MakeLandSession` becomes `FillLand(ac::WorldMap&)`, its elevation loop. Its callers apply it to the fixture's map.
- `AddSessionFaction`, `AddNativeLifeFaction`, `MakeSessionBase` and `MakeSessionUnit` drop their `GameState` parameter and act on the fixture's session.
- These keep a `GameState` of their own:
  - `CurrentGameStateTests`, which tests scopes across states and threads
  - "WorldEvents consumes GameState session RNG", which compares two seeds
  - `CompositionRootTests`, where `Engine` builds the state
- Expected outcome changes: in the ten files with two settings objects, rules and factions now read the same settings.

### 3. Fixture factions join the session

- `FactionFixture::MakeFaction` builds the faction with `state.AllocateFactionId()` and adds it with `state.AddFaction`.
  - The session's first faction is the player's, as now.
  - The hand-wired copies of `AttachToSession_` are deleted.
- `factions` is deleted. Tests iterate `state.Factions()`.
- `MakeFactionBase` and `MakeBase` take their ids from `state.AllocateBaseId()`, and `MakeUnit` from `state.AllocateUnitId()`. `nextFactionId`, `nextBaseId` and `nextUnitId` are deleted.
- `MakeSessionUnit` is deleted. Its callers use `MakeUnit`.
- `GameState::AddFaction` throws `std::invalid_argument` when the faction was built against another map. The comment explaining why it did not is deleted.
- "Unit production without spawn services throws before spawning" builds its faction outside the session, as in step 1.
- Expected outcome changes: fixture factions now get spawn services, tech-discovery effects and a new research target, first contact, production `on_complete` effects and mood notices.

### 4. Fixture bases belong to a session faction

- `MakeFaction` and `MakeFactionBase` move from `FactionFixture` to `BaseFixture`.
- `BaseFixture::MakeBase` calls `MakeFactionBase` on `BaseOwner()`.
  - `BaseOwner()` returns the faction built from `ownerDefinition`. `MakeFaction` adds it the first time `BaseOwner()` is called, so `FactionFixture`'s first faction stays the player's.
  - `pOwnerFaction` and `bases` are deleted.
- Expected outcome changes:
  - A `MakeBase` base's buildings reach its base effects.
  - It claims territory.
  - Its owner has its own faction id, and is the player only when it is the session's first faction.

### 5. Combat and conquest rules drop their context

Each signature below is the new one. Callers pass fewer arguments, and a caller parameter that only fed a dropped argument is deleted too. The rules read the map, the tile effects, the interaction grids, the pathfinder and the mind-control ledger from `CurrentGameState()`.

- `AttackRules`:
  - `bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile)`
  - `Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile)`
  - `Unit* FindAttackableHostileOnTile(const Unit& rAttacker, const Tile& rTargetTile)`
  - `bool CanDeclareAttack(const Unit& rAttacker, const Tile& rTargetTile)`
  - `bool IsWithinBombardRange(const Unit& rAttacker, const Tile& rTargetTile)`
  - `Unit* ArtilleryDuelTarget(const Unit& rAttacker, const Tile& rTargetTile)`
  - `std::vector<Unit*> BombardStrikeTargets(const Unit& rAttacker, const Tile& rTargetTile)`
  - `std::vector<std::string> BombardImprovementTargets(const Tile& rTargetTile)`
- `CombatRules`:
  - `std::vector<CollateralHit_t> PlanStackCollateral(const Unit& rAttacker, const Unit& rDefender)`
  - `std::vector<Unit*> NonCombatantsLeftUnprotected(const Unit& rAttacker, const Unit& rDefender, const std::vector<CollateralHit_t>& rPlan)`
- `InterceptRules`: `std::vector<InterceptSource_t> CollectInterceptSources(const Unit& rAttacker, const Unit& rDefender)`
- `ScrambleRules`: `Unit* FindScrambler(const Unit& rAttacker, Unit& rOriginalDefender)`
- `DiplomaticTransitionRules`: `std::vector<HostileAct_t> BombardHostileActs(const Unit& rAttacker, const Tile& rTarget)`
- `VisibilityRules`: `bool IsUnitVisibleTo(const Faction& rObserver, const Unit& rSubject)`
- `BaseConquestRules`:
  - `bool HasBaseGarrison(const BaseManager& rBase)`
  - `std::optional<ConquestKind_t> ConquestOnEntry(const Unit& rMover, const BaseManager& rBase)`
  - `std::optional<ConquestKind_t> ConquestOnLastDefenderKill(const Unit& rAttacker, const BaseManager& rBase)`

### 6. Movement and placement rules drop their context

- `MovementRules`:
  - `bool CanHoldTileWithoutCarrier(const Unit& rMover, const Tile& rTile)`
  - `bool CanPhysicallyEnterTile(const Unit& rMover, const Tile& rTile)`
  - `bool CanEnterTile(const Unit& rMover, const Tile& rTile)`
  - `bool HasFriendlyOccupant(const Unit& rMover, const Tile& rTile)`
  - `bool HasFriendlyBase(const Unit& rMover, const Tile& rTile)`
  - `bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject)`
  - `bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile)`
- `TransportRules`:
  - `bool CanLoadAtTile(const Unit& rCarrier, const Tile& rTile)`
  - `Unit* FindBoardableTransport(const Unit& rPassenger, const Tile& rTile)`
  - `bool CanUnloadTo(const Unit& rPassenger, const Tile& rFrom, const Tile& rTo)`
  - `bool SurvivesCarrierLoss(const Unit& rPassenger, const Tile& rTile)`
  - `bool CanUnloadTransportInPlace(const Unit& rCarrier)`
- `FuelRules`:
  - `bool IsRefuelSite(const Unit& rUnit)`
  - `EndOfTurnFuel_t ResolveEndOfTurnFuel(const Unit& rUnit)`
  - `bool NeedsAutoReturnToFuel(const Unit& rUnit)`
- `AirdropRules`:
  - `bool TileIsAirdropLaunchPad(const Tile& rTile, int factionId)`
  - `AirdropLanding_t AirdropLanding(const Unit& rUnit, const Tile& rDest)`
  - `bool IsAirdropInterdicted(const Unit& rDropper, const Tile& rDest)`
  - `AirdropEligibility_t CanAirdropTo(const Unit& rUnit, const Tile& rDest)`
- `EvacuateTerritoryRules`:
  - `bool SharesTileWith(const Unit& rUnit, const Faction& rHost)`
  - `const Tile* FindNearestOwnTerritoryTile(const Unit& rUnit)`
- `UnitPlacementRules`: `std::vector<const Tile*> PlacementTiles(const Tile& rOrigin, const UnitDesign& rDesign, FactionId_t factionId, int needed)`
- `FoundBaseRules`:
  - `bool IsTooCloseToAnyBase(const Tile& rTile, const std::vector<const BaseManager*>& rBases)`
  - `bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId, const std::vector<const BaseManager*>& rBases)`

### 7. Ecology, terraform, probe and production rules drop their context

- `EcologyRules`:
  - `BloomCandidates_t BloomCandidateTiles(const Tile& rOrigin)`
  - `std::vector<const NativeUnitConfig_t*> EligibleLifeforms(const Tile& rTile)`, reading the native unit registry from the game data
- `TerraformRules`:
  - `int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId)`
  - `std::optional<TerraformSpread_t> PickSpreadTarget(const Tile& rOrigin)`, reading the improvement registry from the game data
  - `std::vector<ElevationWrite_t> PlanElevationChange(std::span<const ElevationSeed_t> seeds, int floorMeters, int ceilingMeters, const ElevationRulesConfig_t& rRules)`. The elevation rules stay a parameter, because the mutation that plans the change rolls its levels with the same rules.
- `ProbeRules`:
  - `bool CanProbeAction(const Unit& rProbe, const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget)`
  - `std::optional<int> QuoteProbeActionCost(const ProbeActionConfig_t& rAction, const Faction& rActor, const ProbeTarget_t& rTarget)`
  - `ProbeChance_t ProbeChancesFor(const Unit& rProbe, const ProbeTarget_t& rTarget, int risk)`, reading the morale calculator and the probe success formula from the game data. Tests that need another formula set it on `dataContext.probeActionsConfig`.
- `ProductionRules`: `int ItemMineralCost(const BaseManager& rBase, const IConstructable& rItem)`, reading the production config from the game data.

### Docs and rules

- Step 1: in `docs/architecture/high-level.md`, "Current game state" rule 3 says a test's fixture installs the scope for its session.
- Step 3: `docs/architecture/faction-system.md` says `AddFaction` throws for a faction built against another map, where it states the caller contract.
- Step 4: `.devin/rules/testing.mdc` gains a short section, "Fixtures are game sessions": a test runs in its fixture's session, and builds a `GameState` of its own only to compare two sessions.
- Steps 5–7: the architecture docs that name a changed signature are updated in the same commit.

## Tests

- Step 3 adds "AddFaction rejects a faction built against another map".
- In steps 5–7, every test follows the new signatures and keeps what it asserts.
- In steps 1–4, each test whose outcome changes is reported by name, with its cause.
  - If the old outcome held only because the fixture differed from the game, the test's setup changes so it asserts its requirement in a real session.
  - If the new outcome is a game bug, stop and report it.
- Any test that fails is reported by name, with whether it was a requirement change or a bug.

## Verification

- After each commit:
  - `./bd all` (with `--no-sfml` when SFML is unavailable), then `./bd test`.
  - Start the game from the repo root with `timeout 15 stdbuf -oL ./alpha-centauri`. It must print `Starting game loop...` and still be running when `timeout` stops it (exit status 124). Startup runs `LoadGameData` on `config/`, the only check of the shipped config.
- Stop and report any failure.
