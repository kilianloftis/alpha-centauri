# Refactor flags

Rule violations found during the [readable-code cleanup](readable-code-cleanup.md) that need
more than a local fix. They are fixed together in one pass; each entry records its fix.

Each entry looks like this:

```markdown
### `Name` — what is wrong
- **Where:** file and line; callers
- **Problem:** what the code does that breaks the rule.
- **Fix:** the change, and what it touches beyond the function and its callers.
```

## Order of work

- `UnitOrderExecutor` takes `IUnitOrderWorld&` before it is split, and before the
  `FightDefender_` and `TryAirdrop` fixes.
- `CreateBase` loses `bMayOccupyWater` before `BaseManager::BaseManager` is split.
- `Graphics` loses its defaults before `RenderPolicyCell` changes.
- The rest is independent.

## Always-present dependencies passed as nullable

Rule 3. These take a pointer, or look one up and check it, for an object every running game has.

### Effect reference validators — null registries for partial test contexts
- **Where:** `include/game/EffectReferenceValidator.h:21-36` (three `= nullptr` defaults on each
  list overload), the `if (!pRegistry) return;` arms in `src/game/EffectReferenceValidator.cpp`;
  34 calls in `tests/effects/ValidationTests.cpp`
- **Problem:** every registry pointer may be null so tests can validate with part of the data.
  Production always passes all of them, through the `GameDataContext` overload.
- **Fix:** both list overloads leave the header. `EffectPayloadValidator`,
  `TriggeredPayloadValidator` and `ConditionReferenceValidator` hold `const Registry&`. The two
  list walks become `EffectSourceChecker_::Effects` and `Triggered`, so every null arm goes and
  `ValidateEffectReferences(const GameDataContext&)` is the only entry point. The list tests
  build a context with `FillEffectReferenceContext` and load the registries they need. Their
  effects go on `productionConfig->effects` for continuous lists, and on
  `ecoDamageConfig->onPopEffects` for triggered ones. The "a null registry skips the check"
  assertions go with the behaviour.

### `UnitOrderInputController` — a null game state nobody passes
- **Where:** `include/ui/world/UnitOrderInputController.h:25` and the `pGameState ?` branches in
  `src/ui/world/UnitOrderInputController.cpp`
- **Problem:** the only caller (`WorldView.cpp:669`) always passes the session and its
  pathfinder, so every null branch is dead.
- **Fix:** the constructor takes `GameState&` next to the hotkeys, and `HandleMouse` drops
  `pPathfinder` and `pGameState`. The five private handlers read the pathfinder and the probe
  executor from the held session. Every `pGameState ?`, `&& pGameState` and `&& pPathfinder`
  test goes. `WorldView` builds the controller with `m_rGameState`.

### `UnitOrderExecutor` and the terraform mutations — a nullable world
- **Where:** `include/game/units/UnitOrderExecutor.h:52` (`pWorld = nullptr`), the `m_pWorld`
  checks in `src/game/units/UnitOrderExecutor.cpp`, `SetImprovementVisitHandler`;
  `ApplyTerraformResult`, `ApplyElevationDelta`, `ApplyEarthquake`, `LowerTilesOneLevel` and
  `ReconcileSurfaceFlips`, which take `TileEffectsContext*` and `IUnitOrderWorld*`
- **Problem:** `GameState` always passes itself and sets the handler, and every production
  caller of the terraform functions passes the session's tile effects and itself. Only
  `MoraleCalculatorTests` builds an executor without a world, only `TriggeredEffectTests`
  clears the handler, and only `ElevationChangeTests` passes `nullptr, nullptr`.
- **Fix:**
  - The executor takes `IUnitOrderWorld&`, with no default, and every `m_pWorld` check goes.
  - The visit handler becomes `IUnitOrderWorld::OnImprovementVisited(Unit&)`. `GameState`
    implements it with the body of `OnImprovementVisited_`, and the `std::function` and its
    setter go.
  - `ApplyTerraformResult` takes `IUnitOrderWorld&` and loses its "no session" throw.
  - The elevation mutations and `ReconcileSurfaceFlips` take `GameState&` in place of the map,
    the tile effects pointer and the world pointer, since they change state across owners.
    The `if (pTileEffects)` arm and the "no world to raze it" throw go.
  - `MoraleCalculatorTests` uses `fixture.state.GetUnitOrderExecutor()`. The "Unset visit
    handler skips Investigate" case is deleted.
  - The bare-map `ElevationChangeTests` cases run in a `WorldFixture` built with
    `ShippingRules_()` and pass `fixture.state`.

### Requiring the council and the player
- **Where:** `RequireCouncil_` and `RequirePlayer_` in `CouncilVoteView.cpp` and
  `CommlinksView.cpp`, the inline throw in `SatelliteView::OpenAttackerPopup_`,
  `ViewFactory::RequirePlayerFaction_`; the null arms in `InfiltrationRules`, `InterceptRules`,
  `AtrocityRules`, `CouncilFactionVotesPanel`, `EnqueueForPlayer`, `WorldView`, `MinimapDisplay`,
  `InteractionPresenter`, `CommlinksPanel` and `SatelliteView`
- **Problem:** each checks for a council or a player that a running game always has. `Engine`
  always seats a player and always creates the council. The null arms exist because test
  sessions may have neither.
- **Fix:**
  - `GameState` builds the council and its vote executor in its constructor with no members.
    `AddFaction` seats a faction whose identity participates through a new
    `PlanetaryCouncil::Seat`, so the members match today's.
  - `CreatePlanetaryCouncil` goes, along with its call in `Engine` and in six tests.
  - `GetPlanetaryCouncil()` and `GetCouncilVotes()` return references.
  - `GetPlayerFaction()` returns `Faction&` and throws when no faction is player-controlled.
  - The `Require…` helpers and every null arm listed above go.
  - `SessionInputs` absorbs `InstallCouncil`. `OrbitalCombatTests` reloads the fixture
    proposal registry with `council/world_intercept.json`.
  - The first faction a fixture makes is already the player. Tests whose session has only AI
    factions and reaches a player path get a player faction.

## Flag and default parameters

Rule 5. Each needs a type, named constructors or a split that every caller picks from.

### `Faction::CreateBase` and `BaseManager::BaseManager` — `bMayOccupyWater`
- **Where:** `include/game/faction/Faction.h:102`, `include/game/base/BaseManager.h:99`; callers
  in `UnitOrderExecutor.cpp`, `Engine.cpp` and tests; `FoundedBaseMayOccupyWater` in
  `FoundBaseRules.h`
- **Problem:** a flag chooses whether the new base may sit on water.
- **Fix:** `CanFoundBase` already refuses water to any unit that is not a sea unit, so
  `FoundedBaseMayOccupyWater` is "the tile is water". The same holds for `Engine`'s preview
  bases (`rPreview.bWater` is the tile's surface).
  - `BaseManager` sets `m_bMayOccupyWater` from `rTile.IsWater()` at construction.
  - The parameter leaves `CreateBase`, the `BaseManager` constructor and every caller.
  - `FoundedBaseMayOccupyWater` goes.
  - `BaseSnapshot_t` drops `bMayOccupyWater`, so a base built by `CreateBaseFromSnapshot` takes
    it from its tile, as a founding does.
  - The TODO in `Faction::CreateBase` stays. It records that SMAC gives a sea base a free
    Pressure Dome, which also yields as Recycling Tanks, in place of the flag. That is not part
    of this pass.

### `Unit::Unit` and `UnitManager::CreateUnit` — three-valued `pProducedAt`
- **Where:** `include/game/units/Unit.h:49`, `include/game/units/UnitManager.h:48-49`; 6
  `CreateUnit` callers in `src/`, 51 in tests, and `FactionFixture::MakeUnit`
- **Problem:** `std::optional<BaseManager*>` tells three origins apart by `nullopt`, a pointer
  and `nullptr`. The constructor and `CreateUnit` each re-derive the build base, and default
  arguments hide the choice at every call site.
- **Fix:** a `UnitOrigin_t` in `include/game/units/UnitOrigin.h` with a private constructor and
  three named constructors. Each carries the home base, the base whose production grants apply,
  and whether the unit may be a prototype.
  - `Built(BaseManager&)` is base production. The unit is homed there, gets the grants and may
    be a prototype.
  - `Issued(BaseManager&)` is a starting unit. It is homed there and gets the grants, but is not
    a prototype.
  - `Gift(BaseManager* pHome)` is for `GrantUnit`, a fungal bloom or a fleeing garrison. It gets
    no grants.

  The new signatures are `Unit(…, const UnitOrigin_t&)` and `CreateUnit(id, design, positions,
  tile, const UnitOrigin_t&)`, with no defaults. `OnUnitCreated` emits the origin's grant base.
  `MakeUnit` takes a `UnitOrigin_t`.

### `Graphics::DrawText` and `DrawRect` — default colour and thickness
- **Where:** `include/graphics/Graphics.h:97-99`. 3 `DrawText` calls rely on the colour
  default, and 38 `DrawRect` calls on the thickness default.
- **Problem:** `size = 24`, `color = White` and `thickness = 1.0f` are UI style values hidden in
  the graphics interface.
- **Fix:**
  - The defaults leave `Graphics` and its SFML, null and recording implementations.
  - The three `DrawText` calls pass their style section's text colour.
  - Each style section with a `borderColor` gains a required `borderWidth` (in
    `config/ui/style.json`, `tests/fixtures/ui/style.json` and the style parser). Its
    `DrawRect` calls pass it.
  - Calls that outline something else pass that section's existing width field.

### `CollectDestroyableFacilities` and `DestroyRandomFacilities` — two exclusion flags
- **Where:** `include/game/buildings/BuildingRules.h:40-41`, `BuildingMutations.h:27-28`;
  conquest passes literal `false, true`, the DestroyFacility effect passes config fields
- **Problem:** `bExcludeHq` and `bExcludeSecretProjects` choose which facilities qualify.
- **Fix:** `FacilityExclusions_t { bool excludeHq; bool excludeSecretProjects; }` goes in
  `BuildingRules.h`.
  - `DestroyFacilityEffect_t` holds one in place of its two fields, and `TriggeredEffectParser`
    fills it.
  - Both functions take `const FacilityExclusions_t&`.
  - Conquest passes `k_ConquestFacilityExclusions`, a constant in `BaseConquestRules.cpp`.

### `WorldView::SetSelectedUnit_` — manual or automatic selection
- **Where:** `include/ui/world/WorldView.h:125`; 13 calls, most with a literal
- **Problem:** the flag decides whether picking cancels skip-turn and whether the camera
  follows. One caller (`SelectNextAvailableUnit_`) passes the stored mode back in.
- **Fix:** the function splits in two.
  - `PickUnit_(Unit*)` is the player's choice: it sets browse mode and cancels skip-turn.
  - `CycleToUnit_(Unit*)` is automatic: it clears browse mode and centres the camera.
  - The shared part (clearing targeting when the unit changes, and following the unit's tile)
    is a helper both call.
  - The stored-mode call only re-reads the unit's tile, so it becomes
    `SetSelectedTile_(&m_pSelectedUnit->GetTile())`.
  - `m_bManualSelection` stays as the browse state `SelectNextAvailableUnitIfNeeded_` reads.

### `BaseView` — `bEditable`
- **Where:** `include/ui/base/BaseView.h:35`; `ViewFactory` passes whether the base is the
  player's
- **Problem:** the flag decides whether the panels get click callbacks.
- **Fix:** the view derives it from the base it is given:
  `m_bEditable(rBase.GetFaction().IsPlayerControlled())`. The parameter leaves the constructor
  and `ViewFactory::CreateBaseView`.

### `RenderPolicyCell` — `bIsActive`
- **Where:** `src/ui/social-engineering/SocialEngineeringDisplay.cpp:206`; eight parameters
- **Problem:** the flag picks the text colour and whether the active border is drawn.
- **Fix:** it becomes the private member `DrawPolicyCell_(Graphics&, const WindowLayout_t&,
  const SocialPolicyConfig_t&, const SocialPolicyConfig_t* pActivePolicy, float nameRowHeight)`.
  It derives activeness from the policy it is given, and reads the font sizes (`FontSize_`) and
  padding itself. `DrawCategory_` passes the active policy it already holds.

## Entry points only tests reach

Rule 6. Production reaches the same rules another way. Each test moves to the production entry
point or to a fixture file, and then the seam goes.

### `UnitManager::IsPendingDestruction` — used only by a test
- **Where:** `include/game/units/UnitManager.h:61`; `tests/game/UnitPositionTests.cpp:128`
- **Problem:** a query whose only caller is a test checking deferred destruction.
- **Fix:** delete it and its `CHECK`. The `CHECK` after it already reads the deferred unit's id,
  and the map and `Units()` checks cover the logical removal.

### `UnitComponentConfigParser::ParseComponentConfig` — public for tests
- **Where:** `include/game/units/UnitComponentConfigParser.h:22`; called by `ParseConfig` and
  three test files (`ScrapUnitTests`, `CombatRatingTests`, `UnitNamingTests`)
- **Problem:** public only so tests can parse one entry from inline JSON.
- **Fix:** it becomes private. The three tests write their components to a `TempConfigFile`,
  call `ParseConfig` and take the entry by id.

### Session accessors used only by tests
- **Where:** `GameState::SetMissionYear` (12 test callers), `PlayerInteractionQueue::Size` (26),
  `GameSettings::SetSavePath` (3), `GameCategoryToString` (4)
- **Problem:** no production caller.
- **Fix:**
  - `SetMissionYear`: delete it. A fixture helper, `AdvanceToYear(GameState&, int year)` in
    `GameFixtures.h`, calls `IncrementMissionYear` until the year, and throws if the year is
    already past.
  - `Size`: delete it. Tests assert what was queued through `AnyOf` on the payload type,
    `HasPendingFor` or `Empty()`.
  - `SetSavePath`: delete it. Tests point the settings at a temp file with `Load(path)` on a
    missing file, which keeps the defaults and remembers the path.
  - `GameCategoryToString`: delete it with its four `CHECK`s. `ParseGameCategory` stays
    tested.

### Map queries and loaders used only by tests
- **Where:** `ImprovementRegistry::LoadOccupants` (6 test callers) and the two-path
  `LoadTileOccupants` it calls; `RiverConnection_t`, `HasRiverConnection` (4) and
  `GetRiverConnections` (2) in `RiverGeneration.h`; `UnitPositionIndex::SetSingleUnitPerTile`
  (5)
- **Problem:** no production caller. Production never turns the single-unit rule on, so the
  `IsSingleUnitPerTile` branches in `StepEvaluator`, `AirdropRules` and `UnitPositionIndex` only
  run in tests. The renderer draws rivers from `ArtLayer_t::River`, not from the mask.
- **Fix:**
  - `LoadOccupants` and the two-path `LoadTileOccupants` go. Tests call `LoadMapOccupants` with
    a local `TerrainOperationRegistry`, as `SessionInputs` does.
  - `RiverConnection_t`, its operators, both functions and their test case go.
  - The single-unit mode goes, with its setter, getter and branches, and so do the
    five tests that turn it on. `CanPlaceUnit` keeps whatever rules apply when stacking is
    allowed.

### `NativeUnitRegistry::SetFungalBloomLifeforms` — used only by a fixture
- **Where:** `include/game/natives/NativeUnitRegistry.h:18`; `InstallNativeUnits` in
  `GameFixtures.h`
- **Problem:** a setter so tests can change the bloom lifeform range after load.
- **Fix:** delete it. `InstallNativeUnits` reads `native_units.json` and sets the two range
  keys. It writes the result to a `TempConfigFile` and loads that.

### Movement and visibility queries used only by tests
- **Where:** `StepEvaluator::IsTileInHostileZoc` (18 test callers), `StepEvaluator::IsZocViolation`
  (3), `ApplyRemoveFog` in `VisibilityMutations.h` (1) and the `Faction::SetFogRemoved` /
  `IsFogRemoved` latch only it sets
- **Problem:** no production caller. Production reaches the ZOC rules through `EvaluateStep`.
  Nothing triggers the one-shot fog removal.
- **Fix:**
  - The two public ZOC wrappers go. Each assertion that a tile is in hostile ZOC becomes a step
    into it, from another ZOC tile, that `EvaluateStep` reports as `BlockedByZoc`. Each
    assertion that a tile is outside ZOC becomes a legal step.
  - `ApplyRemoveFog` and the fog latch go. Its test turns fog off through the `RemoveFog`
    rule flag and `ApplyVisibilityRules`.

### Combat queries used only by tests
- **Where:** `CanDeclareAttack` in `include/game/combat/AttackRules.h` (14 test callers),
  `MoraleCalculator::LevelEffects` (2)
- **Problem:** no production caller. Production declares attacks through
  `FindAttackableHostileOnTile`.
- **Fix:**
  - `CanDeclareAttack` goes. Its tests check that `FindAttackableHostileOnTile` returns the
    expected defender, or null.
  - `LevelEffects` goes. Its level-4 assertion reads `FindLevel(4)->effects` from the parsed
    fixture `MoraleConfig_t`, and its missing-level throw case is deleted.

### Base and building queries used only by tests
- **Where:** `BaseManager::GetMineralSupportCost` (2), `GetDroneModifier` (3),
  `GetTalentModifier` (3); `BuildingConfig_t::GetUpkeep` (1); `BuildingUpkeepLine_t::UpkeepPerCopy`
  (6); `ResolveFacilityEnergyUpkeepPerCopy` (7); `CountFactionOrbitalBuildings` (6)
- **Problem:** no production caller, except `ResolveFacilityEnergyUpkeepPerCopy`. It is public
  only for tests; production reaches it inside `TallyBuildingUpkeepByType`.
- **Fix:**
  - `GetMineralSupportCost`, a pass-through, goes. Tests call `MineralSupportCost(base)`.
  - `GetDroneModifier` and `GetTalentModifier` go. Tests read `dronePressure` and
    `resolvedTalents` from `BuildCompositionInputs(base, drones)` before and after the
    facility.
  - `GetUpkeep` and `UpkeepPerCopy` go. Tests read the public `upkeep` and `upkeepPerCopy`
    fields.
  - `ResolveFacilityEnergyUpkeepPerCopy` moves into `BuildingUpkeep.cpp`'s anonymous namespace.
    Its tests go through `TallyBuildingUpkeepByType`.
  - `CountFactionOrbitalBuildings` goes. Tests find the entry in `BuildOrbitalCensus()`.

### Population helpers used only by tests
- **Where:** `AssimilationTracker::IsAssimilating` (10 test callers), `Pop::GetDroneWeight` (2);
  `MakeGrowthBaselineStat`, `MakeDefaultGrowthEffects` and the `GrowthConfig_t` member defaults
  in `include/game/population/GrowthConfigParser.h`
- **Problem:** no production caller. The growth helpers and defaults exist so tests can build a
  growth config by hand, which `calculator-config.mdc` says should come from a fixture.
- **Fix:**
  - `IsAssimilating` goes. Tests read `OccupierWindow().IsActive()`.
  - `GetDroneWeight` goes. Tests read `GetConfig().droneWeight`.
  - The growth helpers go, and the `GrowthConfig_t` members get no defaults. `SessionInputs`
    parses `tests/fixtures/pop_growth.json`. The tests that build a `GrowthConfig_t` parse the
    same fixture and adjust it with `SetGrowthBaseline`, which builds its own `StatModifier`.

### `UIManager::PopView` — used only by tests
- **Where:** `include/ui/UIManager.h:36`; 2 test callers
- **Problem:** views close themselves; nothing in production pops one.
- **Fix:** delete it. The tests' fake overlay sets its close flag, and the manager prunes it on
  the next `ProcessInput`, as it does every closing view.

### World view accessors used only by tests
- **Where:** `MapViewport::TileWidth` and `TileHeight` (3 test callers each),
  `ResolveMapOverlayPathTemplate` in `FactionBaseArt.h` (1)
- **Problem:** no production caller. `TileWidth` repeats `TileSize`.
- **Fix:**
  - `TileWidth` and `TileHeight` go. Tests use `TileSize()`, and `TileSize() / 2` for the
    height.
  - `ResolveMapOverlayPathTemplate` goes with its `CHECK`. `SubstituteMapOverlayFaction` stays
    tested.

### `BaseView::GetSnapshotBuildCount` and `CouncilVoteWeightCache::GetComputeCount` — counters for tests
- **Where:** `include/ui/base/BaseView.h:46` (3 calls in `BaseViewTests.cpp`),
  `include/ui/council/CouncilVoteWeightCache.h:28` (3 test callers)
- **Problem:** they exist so tests can see that a cache saved work.
- **Fix:** both counters go, with the assertions that read them. The tests keep
  what a player can see: an unchanged base draws the same, and a changed base or council input
  draws the new value.

## Code waiting on a feature

Rule 6. These have no production caller because the feature that would call them is not built.
They stay until that feature lands, and this list records what is left to build. This pass does
not change them.

### `AIProfile` — tendencies nothing reads
- **Where:** `include/game/faction/AIProfile.h`; `Faction::m_pAIProfile`
- **Problem:** the class only holds the parsed `ai.json` tendencies for an AI that does not
  exist.
- **Waits on:** the AI.

### `EventBus::Unsubscribe` and `ScopedConnection::IsConnected` — used only by tests
- **Where:** `include/lib/EventBus.h:29`, `include/lib/Signal.h:73`; `tests/lib/EventBusTests.cpp`,
  `tests/lib/SignalTests.cpp`
- **Problem:** no production caller; the tests check unsubscribe semantics for mod handlers that
  do not exist yet.
- **Waits on:** mod event handlers.

### Faction save/load and flavour paths used only by tests
- **Where:** `Faction::ExtractBase` (7 test callers), `Faction::CreateBaseFromSnapshot` (2),
  `BaseManager::CaptureSnapshot` and `BaseSnapshot_t`, `FactionFlavor::PickPhrase` (2),
  `ResearchSelector::SetCategoryEnabled` (20)
- **Problem:** no production caller.
- **Waits on:** save/load (the snapshot pair), diplomacy dialogue (`PickPhrase`) and the research
  priority UI (`SetCategoryEnabled`).

### Production and economy entry points used only by tests
- **Where:** `ComputeCommerceForFaction` and `ComputeCommerceForBase` in `CommerceRules.h` (17
  test callers each), `EconomyManager::SetEnergyAllocation` (7), `ResourceManager::GetPsych` (5),
  `WorkerAssignmentManager::SetTileScorer` (1), `ProductionManager::Replace` and `Delete` (2)
- **Problem:** no production caller. Production reaches commerce through `CommerceCache` and
  `ComputeCommerceLines`.
- **Waits on:** the energy sliders (`SetEnergyAllocation`), build-queue editing (`Replace`,
  `Delete`) and the worker governor (`SetTileScorer`). The commerce functions and `GetPsych`
  have no feature of their own, so their tests can move to `ComputeCommerceLines` and the
  getter production reads whenever these are next touched.

### Diplomacy, council and atrocity queries used only by tests
- **Where:** the whole `DiplomaticActionExecutor` (`Propose`, `Accept`, `Reject`,
  `GetPendingProposal`) and `GameState::GetDiplomaticActionExecutor` (5); `AtrocityLedger::HasVictimized`
  (6), `HasCommittedMajorAgainst` (5), `SanctionUntilYear` (4); `PenaltiesApplied` in
  `AtrocityMutations.h` (18); `CouncilVoteExecutor::VetoPending` (3), `AllMembersVoted` (3);
  `PlanetaryCouncil::GetSupremeLeaderVictoryStub` (2); `GetAvailableActions` in
  `DiplomacyActions.h` (5); `DiplomacyLedger::AddGrievance` (1), `AddIntegrity` (1);
  `MayRepairAt` (4); `TradeKinds` (1)
- **Problem:** no production caller. Nothing proposes a diplomatic action, so the executor is
  unwired end to end.
- **Waits on:** the diplomacy UI and AI (the executor, the ledger queries, `GetAvailableActions`,
  `TradeKinds`), the council UI (`VetoPending`, `AllMembersVoted`), the diplomatic victory
  check (`GetSupremeLeaderVictoryStub`) and per-turn healing (`MayRepairAt`).

## Config parsers and loaders

Rule 7. Each parses a whole file or every kind in one function. The fix is one parser per
section or kind, registered the way the effect types are (`EffectTypeParsers_`).

### `EffectConfigParser` — per-kind parsing in one function each
- **Where:** `src/game/effects/EffectConfigParser.cpp`: `ParseCondition` (247 lines),
  `ValidateAmountSourceLegality_` (208), `ParseInteractionOverride_` (154),
  `ValidateEffectForSource` (126), `ParseStatModifier_` (105)
- **Problem:** each walks every kind in one if-chain.
- **Fix:**
  - `ParseCondition` dispatches through a `ConditionParsers_()` table, kind name to
    `Parse<Kind>Condition_`, built like `EffectTypeParsers_`. An unknown or missing `kind`
    throws, so `.value("kind", "")` becomes `.at("kind")`.
  - `ValidateAmountSourceLegality_` keeps its switch, and each case calls one
    `Validate<Source>Source_`.
  - `ParseInteractionOverride_` reads the grid through one explicit string map next to
    `InteractionGridId_t`. Its remaining fields become named steps.
  - `ValidateEffectForSource` becomes a run of `Require…_` checks, one per source rule.
  - `ParseStatModifier_` splits the `move_cost` form, `amount_source` and the plain amount
    into named steps.
  - Error messages stay the same, so the parser tests stay unchanged.

### `TileRenderer::DrawLinkNetworks_`
- **Where:** `src/ui/TileRenderer.cpp` (`DrawLinkNetworks_`, 99 lines, complexity 28)
- **Problem:** per-neighbour link, replacement and hub drawing all happen inline.
- **Fix:** a `LinkNetworks_t` holds `networks`, `neighbors` and `links`. The steps become
  `CollectLinkNetworks_`, `MarkLinks_`, `ApplyLinkReplacements_`, `DrawLinks_` (which returns
  whether any link was drawn) and `DrawHubs_`. Screenshots of a road, mag-tube and base map are
  taken before and after, and must match pixel for pixel.

## Variant dispatch

Rule 7. The fix is an exhaustive visitor struct per variant, as `EffectReferenceValidator` uses.

### `ConditionSatisfied` and `ApplyOne_` — one std::visit lambda over every alternative
- **Where:** `src/game/effects/ActiveEffect.cpp` (`ConditionSatisfied(const Condition_t&, …)`,
  176 lines), `src/game/effects/TriggeredEffectDispatch.cpp` (`ApplyOne_`, 192 lines);
  `AmountSourceValue(const StatModifierEffect_t&, …)` (94) has the same shape over a switch
- **Problem:** an `if constexpr` chain inside one lambda handles every alternative.
- **Fix:**
  - `ConditionSatisfied` visits a `ConditionEvaluator_ { ctx, pOriginBase }` with one
    `operator()` per alternative. `AllOf` and `AnyOf` recurse through it.
  - `ApplyOne_` visits a `TriggeredEffectApplier_ { rConfig, rCtx, rFaction, rOut }`. Arms that
    already call a helper forward to it, and the inline arms (`GrantEnergy`, `WorldParameter`
    and the rest) become named helpers.
  - Each `AmountSourceValue` case calls one `<Source>Amount_`.

## Long UI and simulation functions

Rule 7. Each splits into named steps. SMAC ports are checked against the disassembly or
screenshots.

### `WorldView` order dispatch and construction
- **Where:** `src/ui/world/WorldView.cpp`: `ApplyOrderInput_` and `ApplyOrderClick_` (the
  `Was…Requested` chains), the constructor (eleven parameters);
  `UnitOrderInputController`'s eleven request flags
- **Problem:** each recognised order is a flag the view tests in a fixed chain.
- **Fix:**
  - The controller's flags and `Was…Requested` getters become one
    `std::optional<UnitOrderRequest_t>` per event, holding a `UnitOrderRequestKind_t` and an
    optional target tile. The key requests come from `ApplyOrderAction_`, and the hold-release
    requests (attack, bombard, probe, order assigned) come from the mouse handlers.
  - `WorldView` dispatches with one switch over the kind, each case one call.
    `ApplyOrderClick_` keeps its focus-the-target step.
  - The five constructor callbacks become a `WorldViewCallbacks_t` struct.

### `moisture_gen::GenerateRainfall`
- **Where:** `src/game/worldgen/MoistureGeneration.cpp`, 306 lines, nesting 9
- **Problem:** the whole Thinker world_rainfall port is one function.
- **Fix:** a `RainfallGrids_t` holds `alts`, `temps` and `flags`. The passes become
  `SampleTiles_`, `MarkOceanMoisture_` (the cloudmass and ocean pass), `ScanRainBelts_` and
  `SmoothAridNeighbors_`, each checked against Thinker's `world_rainfall` while it is split.
  Before the split, a test records the moisture grid for a fixed seed and preset. The split
  must reproduce that grid exactly, and the test stays as the port's regression guard.

### `BaseManager::BaseManager`
- **Where:** `src/game/base/BaseManager.cpp`, 52 lines, eight parameters
- **Problem:** the member initializer list builds every subsystem with nested calls.
- **Fix:**
  - Static helpers `MakePopulation_(rRules, rFaction, *this, initialPopulation)`,
    `MakeWorkerAssignments_(rTileEffects, tile, population)` and `MakeProduction_()` build the
    subsystems.
  - The stockpile fallback lambda becomes the member `FallbackProduction_() const`.
  - The constructor body's signal hookups become `ConnectSignals_()`.
  - With `bMayOccupyWater` gone, there are seven parameters.

## Duplicated logic

Each duplicate becomes one shared function.

### Required-tech availability, in eight places
- **Where:** `UnitComponentConfig_t::IsAvailable`, `UnitSlotConfig_t::IsAvailable`,
  `BuildingConfig_t::IsAvailable`, `SocialPolicyConfig_t::IsAvailable`,
  `CouncilProposalConfig_t::IsAvailable`, `StockpileConfig_t::IsAvailable`,
  `PopTypeAvailabilityCalculator.cpp:18`, `HasRequiredTech_` in `TerraformRules.cpp`, and the
  inline check in `ProbeRules.cpp:150`
- **Problem:** each repeats "an empty `requiredTech`, or one that is discovered".
- **Fix:** `bool MeetsTechRequirement(const TechId& requiredTech, const std::vector<TechId>&
  rDiscovered)` goes in `include/game/research/TechRequirement.h`.
  - Each `IsAvailable` becomes a one-line call to it.
  - `PopTypeAvailabilityCalculator`, `TerraformRules` and `ProbeRules` call it with
    `GetResearch().GetDiscoveredTechs()`. `HasRequiredTech_` and the pop-type set copy go.

## Single responsibility

Each of these classes splits into parts that each touch their own members. The architecture
diagrams in `docs/architecture/` are updated with each split.

### `UnitOrderExecutor`
- **Signals:** "and": it steps, attacks, bombards, founds bases, terraforms and airdrops. The
  `.cpp` includes 43 headers from ten domains. Its private helpers group into movement, combat
  and orders.
- **Fix:** three executors, all owned by `GameState`.
  - `UnitMoveExecutor` takes `TryStep`, `TryAirdrop`, `EnterTile_`, `SpendMovesAndEnter_`,
    `ApplyArrivalEffects_`, `RevealBlockingUnits_` and the visible-hostile set.
  - `UnitAttackExecutor` takes `TryAttack`, `TryBombard`, `TryIntercept_`,
    `ResolveScrambleDefender_`, `ScrambleTo_`, `FightDefender_`, `FireBombard_`,
    `PromoteOnKill_`, `SpendAttackAction_`, `ApplyLastDefenderConquest_` and
    `DestroyRandomImprovement_`. It holds the `CombatResolver` and calls the move executor for
    scrambles.
  - `UnitOrderExecutor` keeps `Execute`, `OnTurnStart`, the per-order `Execute_` overloads,
    `TryFoundBase` and terraforming, and calls the other two.
  - `GameState` gains `GetUnitMoves()` and `GetUnitAttacks()`, and callers of `TryStep`,
    `TryAttack`, `TryBombard` and `TryAirdrop` use them.

### `Unit`
- **Signals:** about 50 public methods in groups that touch disjoint members: cargo
  (`m_pCarrier`, `m_cargo`), supply crawl (`m_tileClaim`, `m_order`), per-turn latches (attack
  history, airdrop, intercept cooldown) and moves and fuel.
- **Fix:**
  - `UnitTransport` holds the carrier and cargo links: `IsEmbarked`, `GetCarrier`, `GetCargo`,
    `EmbarkInto`, `Disembark` and `ClearCargoLinks_`.
  - `UnitTurnLatches` holds the attack history, the airdrop latch and the intercept cooldown,
    with `Roll()` for `BeginTurn`.
  - `Unit` exposes both as `Transport()` and `Latches()`, and callers go through them.
  - `TryStartSupplyCrawl` is a mechanic decision, so it moves to `SupplyCrawlRules` and
    `SupplyCrawlMutations` (`code-kinds.mdc`). `Unit` keeps the claim and gains
    `AdoptSupplyCrawl(WorkedTileClaim, StatId_t)` to hold it and set the order together.

## Noted during the abstraction-level pass

Found while applying rules 7 and 8.

### `UnitOrderExecutor::FightDefender_` — a redundant base check
- **Where:** `src/game/units/UnitOrderExecutor.cpp`, `bDefenderOnBase` in `FightDefender_`
- **Problem:** the attacker only conquers when the defender stood on a base, checked before
  combat. `ResolvePostCombatBaseConquest` already returns nothing without a base. The comment's
  reason (the tile reference is freed) does not hold, since tiles belong to the map. `TryBombard`
  makes the same call without the check.
- **Fix:** with `IUnitOrderWorld&` required, `IUnitOrderWorld::ResolvePostCombatBaseConquest`
  documents that it returns nothing without a base. `bDefenderOnBase` and its comment go.

### `UnitOrderExecutor::TryAirdrop` — a mechanic decision in an executor
- **Where:** `src/game/units/UnitOrderExecutor.cpp`, `landing.hpAfter < rUnit.GetCurrentHp()`
- **Problem:** the landing only ever lowers hit points. That is a decision `AirdropLanding`
  should make, per `code-kinds.mdc`.
- **Fix:** `AirdropLanding` caps `hpAfter` at the unit's current hit points, and the executor
  sets it unconditionally. `AirdropTests` asserts that a landing never raises hit points.

### `PlaceTestImprovementsAroundBases_` — the same nearest-host search twice
- **Where:** `src/game/Engine.cpp`, under `#ifdef AC_PLACE_TEST_IMPROVEMENTS`
- **Problem:** the improvement and farm loops each write out the same nearest-tile search with a
  different filter.
- **Fix:**
  - `FindNearestHost_(rBaseTile, rMap, filter)` does the search, and the two loops become
    `PlaceOneOfEachImprovement_` and `PlaceMoistureFarms_`.
  - The block is built once with `-DAC_PLACE_TEST_IMPROVEMENTS` to prove it compiles, then
    left off.

### Level-set parsers and `RejectUnknownKeys_` — the same parse in several files
- **Where:** `src/game/DifficultyConfigParser.cpp` and `src/game/natives/NativeLifeLevelConfig.cpp`
  (`default` plus a list of uniquely-id'd levels); `ParseSeverities_` and `ParseStatuses_` in
  the atrocity and diplomacy parsers (every enum value exactly once); `RejectUnknownKeys_` is
  copied into nine parsers (`grep -rl 'void RejectUnknownKeys_' src`)
- **Problem:** each file writes out the same default-and-levels check and its own key whitelist,
  so their messages and rules can drift apart.
- **Fix:** three shared forms go in `lib/config`.
  - `RejectUnknownKeys(json, known, context)` replaces the nine copies.
  - `ParseLevelSet<Level_t>(json, context, parseLevel)` returns the default id and the levels,
    checking that the default exists and the ids are unique. The difficulty and native-life
    parsers use it.
  - `ParseEnumKeyedObject<Enum_t, Entry_t>(json, what, context, parseEntry)` returns one entry
    per enum value and throws on a repeated or missing value. The atrocity and diplomacy
    parsers use it.

  Every message comes from these, so the parser tests that match message text are updated to
  the shared wording.

## Divergences from SMAC

Found while checking the sea-base rules against `terranx.exe`.

### Submerged bases and units — razed outright, units untouched
- **Where:** `SurfaceFlipConsequences` in `src/game/terraform/TerraformRules.cpp:529`,
  `ReconcileSurfaceFlips` in `src/game/terraform/SurfaceOccupancy.cpp`; the surface-flip cases
  in `tests/game/FoundBaseTests.cpp:373-480`; `docs/architecture/map-system.md` and
  `docs/mechanic-decisions-review.md:542`, which describe the current rule
- **Problem:**
  - When a tile under a base without a Pressure Dome becomes water, we raze the base.
    `alt_set` (`docs/thinker/terranx-map.md`, "Sea bases") floods it instead. The base loses
    `clamp(pop / 2, roll 2–4, 10)` pop and is granted a Pressure Dome. It is only destroyed
    when that leaves no pop and it is not the faction's last base. The last base is kept at
    pop 1.
  - We never touch the units on a flipped tile. `alt_set` kills every non-air unit whose
    domain no longer matches the surface, unless a base on the tile survived the flip.
- **Fix:**
  - `ElevationRulesConfig_t` gains three required keys in `config/map_rules.json` and
    `tests/fixtures/map_rules.json`: `submerged_pop_loss_roll_min` (2),
    `submerged_pop_loss_roll_max` (4) and `submerged_pop_loss_cap` (10).
  - `SurfaceFlipConsequences` decides the outcome. For a base without a dome on a tile that
    became water, it returns a `SubmergedBase_t`: the roll range, the capped half-pop loss,
    and whether this is the faction's last base. It also returns the units to kill: every
    non-air unit on the tile whose domain differs from the new surface. The list is empty
    when a base on the tile is unaffected or floods and survives.
  - `ReconcileSurfaceFlips` applies the outcome:
    - It draws the roll and removes `max(roll, half-pop loss)` pops with
      `PopulationManager::RemovePop`.
    - At 0 pop, the faction's last base keeps one pop. Any other base is razed, and its
      wrong-domain units are killed.
    - A surviving base gets `Pressure_Dome` through `AddBuilding`.
    - It kills the listed units with `DestroyUnit`.
  - The player gets a `NoticeInteraction_t` when one of their bases is flooded or destroyed,
    or when a base of another faction that they can see is. This mirrors the
    `BASESUBMERGED` popups.
  - Objective bases have no counterpart here (there are no scenarios). A TODO in the rule
    names `is_objective`.
  - `FoundBaseTests`' "razed unless it may occupy water" case becomes one case per outcome:
    domed base kept, flooded base shrinks and gains a dome, emptied base razed, last base kept
    at pop 1, and units killed or sheltered.
  - The two docs are updated to the new rule.

