# Refactor flags

Rule violations found during the [readable-code cleanup](readable-code-cleanup.md) that need
more than a local fix. Each one gets a dedicated plan later. Until then the code stays as it is.

Entries are grouped by theme, and each theme gets its own plan. Each entry looks like this:

```markdown
### `Name` — what is wrong
- **Where:** file and line; callers
- **Problem:** what the code does that breaks the rule.
- **Why not local:** what a fix touches beyond the function and its callers.
```

## Always-present dependencies passed as nullable

Rule 3. These take a pointer, or look one up and check it, for an object every running game has. The
plan: pass references, and give the tests that build partial sessions a full fixture session.

### Effect reference validators — null registries for partial test contexts
- **Where:** `include/game/EffectReferenceValidator.h:21-36` (three `= nullptr` defaults on each
  list overload), the `if (!pRegistry) return;` arms in `src/game/EffectReferenceValidator.cpp`;
  34 callers
- **Problem:** every registry pointer may be null so tests can validate with part of the data;
  production always passes all of them through the `GameDataContext` overload.
- **Why not local:** the list overloads should take the context (or references), and the tests
  that pass partial registries need a fixture context.

### `UnitOrderInputController` — a null game state nobody passes
- **Where:** `include/ui/world/UnitOrderInputController.h:25` and the `pGameState ?` branches in
  `src/ui/world/UnitOrderInputController.cpp`
- **Problem:** the only caller always passes the session, so every null branch is dead.
- **Why not local:** it threads through five private handlers, which would all take
  `GameState&`.

### `UnitOrderExecutor` — null `pWorld` and an unset visit handler
- **Where:** `include/game/units/UnitOrderExecutor.h:52` (`pWorld = nullptr`), the `m_pWorld`
  checks in `src/game/units/UnitOrderExecutor.cpp`, `SetImprovementVisitHandler`
- **Problem:** `GameState` always passes itself and sets the handler. Only
  `MoraleCalculatorTests` builds an executor without a world, and only `TriggeredEffectTests`
  clears the handler; the null branches exist for them.
- **Why not local:** those tests need a session, after which the executor can take
  `IUnitOrderWorld&`.

### Requiring the council and the player, in three views
- **Where:** `RequireCouncil_` and `RequirePlayer_` in `CouncilVoteView.cpp` and
  `CommlinksView.cpp`; the inline throw in `SatelliteView::OpenAttackerPopup_`
- **Problem:** each view checks for a council and a player that a running game always has.
- **Why not local:** `GameState` could hand out references once every session has both, which
  changes tests that build a session without a council.

## Flag and default parameters

Rule 5. Each needs a type, named constructors or a split that every caller picks from. The plan: one
change per entry, smallest caller count first.

### `Faction::CreateBase` — `bMayOccupyWater`
- **Where:** `include/game/faction/Faction.h:102`; callers in `UnitOrderExecutor.cpp`,
  `Engine.cpp` and tests
- **Problem:** a flag chooses whether the new base may sit on water.
- **Why not local:** `UnitOrderExecutor` computes it with `FoundedBaseMayOccupyWater(rUnit)`;
  deriving it inside `CreateBase` needs the founding unit, which the other callers do not have.

### `Unit::Unit` and `UnitManager::CreateUnit` — three-valued `pProducedAt`
- **Where:** `include/game/units/Unit.h:49`, `include/game/units/UnitManager.h:48-49`; 12
  `CreateUnit` callers in `src/`, 51 in tests
- **Problem:** `std::optional<BaseManager*>` tells "built at the home base", "built at this base"
  and "a gift" apart by `nullopt`, a pointer and `nullptr`. The constructor and `CreateUnit` each
  re-derive the build base, and default arguments hide the choice at every call site.
- **Why not local:** the fix is a type or named constructors for the three origins, and every
  caller picks one.

### `Graphics::DrawText` and `DrawRect` — default size, colour and thickness
- **Where:** `include/graphics/Graphics.h:97-99`; 112 `DrawText` calls and 31 `DrawRect` calls,
  most relying on the defaults
- **Problem:** `size = 24`, `color = White` and `thickness = 1.0f` are UI style values hidden in
  the graphics interface.
- **Why not local:** each caller should take its size, colour and thickness from its style, which
  touches every UI slice.

### `CollectDestroyableFacilities` and `DestroyRandomFacilities` — two exclusion flags
- **Where:** `include/game/buildings/BuildingRules.h:40-41`, `BuildingMutations.h:31-32`;
  conquest passes literal `false, true`, the DestroyFacility effect passes config fields
- **Problem:** `bExcludeHq` and `bExcludeSecretProjects` choose which facilities qualify.
- **Why not local:** an exclusion policy type would be shared with `DestroyFacilityEffect_t`,
  which already carries the same two fields.

### `WorldView::SetSelectedUnit_` — manual or automatic selection
- **Where:** `include/ui/world/WorldView.h:87`; 13 calls, most with a literal
- **Problem:** the flag decides whether picking cancels skip-turn and whether the camera
  follows; one caller passes the stored mode back in.
- **Why not local:** splitting it into manual and automatic selection needs that caller to
  branch on the stored mode, and the mode is itself view state worth a look.

### `BaseView` — `bEditable`
- **Where:** `include/ui/base/BaseView.h:35`; `ViewFactory` passes whether the base is the
  player's
- **Problem:** the flag decides whether the panels get click callbacks.
- **Why not local:** the view would derive it from the base's owner and the player faction, which
  it does not hold; or `ViewFactory` would build two named kinds of view.

### `RenderPolicyCell` — `bIsActive`
- **Where:** `src/ui/social-engineering/SocialEngineeringDisplay.cpp:206`; eight parameters
- **Problem:** the flag picks the text colour and whether the active border is drawn.
- **Why not local:** it belongs with splitting `SocialEngineeringDisplay::Render` (below), which
  builds every cell inline.

## Entry points only tests reach

Rule 6. Production reaches the same rules another way. The plan: move each test to the production
entry point or to a fixture file, then delete the seam.

### `UnitManager::IsPendingDestruction` — used only by a test
- **Where:** `include/game/units/UnitManager.h:61`; `tests/game/UnitPositionTests.cpp:128`
- **Problem:** a query whose only caller is a test checking deferred destruction.
- **Why not local:** the test has to assert the behavior through `Units()` and occupancy instead.

### `UnitComponentConfigParser::ParseComponentConfig` — public for tests
- **Where:** `include/game/units/UnitComponentConfigParser.h:21`; called by `ParseConfig` and
  three test files (`ScrapUnitTests`, `CombatRatingTests`, `UnitNamingTests`)
- **Problem:** public only so tests can parse one entry from inline JSON.
- **Why not local:** those tests have to go through `ParseConfig` with a temp file.

### Session accessors used only by tests
- **Where:** `GameState::SetMissionYear` (12 test callers), `GameState::GetDiplomaticActionExecutor`
  (5), `PlayerInteractionQueue::Size` (26), `GameSettings::SetSavePath` (3),
  `GameCategoryToString` (4)
- **Problem:** no production caller.
- **Why not local:** each needs its tests rewritten against behavior, or a fixture seam.

### Map queries and loaders used only by tests
- **Where:** `ImprovementRegistry::LoadOccupants` (6 test callers) and the two-path
  `LoadTileOccupants` it calls; `HasRiverConnection` (4) and `GetRiverConnections` (2) in
  `RiverGeneration.h`; `UnitPositionIndex::SetSingleUnitPerTile` (5)
- **Problem:** no production caller.
- **Why not local:** fixtures load occupants this way and tests flip the stacking rule; both
  need a fixture path through `LoadMapOccupants` and the session config.

### `NativeUnitRegistry::SetFungalBloomLifeforms` — used only by a test
- **Where:** `include/game/natives/NativeUnitRegistry.h:18`; one test caller
- **Problem:** a setter so a test can change the bloom lifeform list after load.
- **Why not local:** the test needs a fixture native_units config with that list.

### Movement and visibility queries used only by tests
- **Where:** `StepEvaluator::IsTileInHostileZoc` (18 test callers), `StepEvaluator::IsZocViolation`
  (3), `ApplyRemoveFog` in `VisibilityMutations.h` (1)
- **Problem:** no production caller; production reaches these rules through `EvaluateStep` and
  `ApplyVisibilityRules`.
- **Why not local:** the tests assert the ZOC and fog rules through these entry points and
  would move to the production ones.

### Combat queries used only by tests
- **Where:** `CanDeclareAttack` in `include/game/combat/AttackRules.h` (14 test callers),
  `MoraleCalculator::LevelEffects` (2)
- **Problem:** no production caller; production declares attacks through
  `FindAttackableHostileOnTile`.
- **Why not local:** the tests assert attack legality through it and would move to the
  production entry point.

### Base and building queries used only by tests
- **Where:** `BaseManager::GetMineralSupportCost` (2), `GetDroneModifier` (3),
  `GetTalentModifier` (3); `BuildingConfig_t::GetUpkeep` (1); `BuildingUpkeepLine_t::UpkeepPerCopy`
  (6); `ResolveFacilityEnergyUpkeepPerCopy` (7); `CountFactionOrbitalBuildings` (6)
- **Problem:** no production caller.
- **Why not local:** each needs its tests rewritten against behaviour.

### Population helpers used only by tests
- **Where:** `AssimilationTracker::IsAssimilating` (10 test callers), `Pop::GetDroneWeight` (2);
  `MakeGrowthBaselineStat`, `MakeDefaultGrowthEffects` and the `GrowthConfig_t` member defaults
  in `include/game/population/GrowthConfigParser.h`
- **Problem:** no production caller; the growth helpers and defaults exist so tests can build a
  growth config by hand, which `calculator-config.mdc` says should come from a fixture.
- **Why not local:** the tests that build `GrowthConfig_t` in code need a fixture file.

### `UIManager::PopView` — used only by tests
- **Where:** `include/ui/UIManager.h:36`; 2 test callers
- **Problem:** views close themselves; nothing in production pops one.
- **Why not local:** the tests would close views the way production does.

### World view accessors used only by tests
- **Where:** `MapViewport::TileWidth` and `TileHeight` (3 test callers each),
  `ResolveMapOverlayPathTemplate` in `FactionBaseArt.h` (1)
- **Problem:** no production caller.
- **Why not local:** the tests would assert through rendering instead.

### `BaseView::GetSnapshotBuildCount` — used only by tests
- **Where:** `include/ui/base/BaseView.h`; 3 calls in `BaseViewTests.cpp`
- **Problem:** it exists so tests can see the snapshot was not rebuilt.
- **Why not local:** the tests need another way to observe the avoided rebuild, such as a
  counting snapshot builder.

### `CouncilVoteWeightCache::GetComputeCount` — used only by tests
- **Where:** `include/ui/council/CouncilVoteWeightCache.h:28`; 3 test callers
- **Problem:** it exists so tests can see the cache saved work.
- **Why not local:** the tests need another way to observe the skipped resolves.

## Code waiting on a feature

Rule 6. These have no production caller because the feature that would call them is not built. The
plan: decide per feature whether to delete now or keep for it, and record the decision in the
feature's doc.

### `AIProfile` — tendencies nothing reads
- **Where:** `include/game/faction/AIProfile.h`; `InterestedInWealth`, `InterestedInPower`,
  `InterestedInGrowth` and `InterestedInTech` have no caller
- **Problem:** the class only holds the parsed `ai.json` tendencies for an AI that does not exist.
- **Why not local:** removing it touches `Faction`, the faction config and the parser; it may be
  wanted once AI lands.

### `EventBus::Unsubscribe` and `ScopedConnection::IsConnected` — used only by tests
- **Where:** `include/lib/EventBus.h:29`, `include/lib/Signal.h:73`; `tests/lib/EventBusTests.cpp`,
  `tests/lib/SignalTests.cpp`
- **Problem:** no production caller; the tests check unsubscribe semantics for mod handlers that
  do not exist yet.
- **Why not local:** removing them drops those tests' subject; decide with the mod seams.

### Faction save/load and flavour paths used only by tests
- **Where:** `Faction::ExtractBase` (7 test callers), `Faction::CreateBaseFromSnapshot` (2),
  `FactionFlavor::PickPhrase` (2), `ResearchSelector::SetCategoryEnabled` (20)
- **Problem:** `CreateBaseFromSnapshot` and `ExtractBase` exist for a save/load that is not
  built; the others are reached only from tests.
- **Why not local:** each needs its tests rewritten, and the snapshot pair is a save/load design
  question.

### Production and economy entry points used only by tests
- **Where:** `ComputeCommerceForFaction` and `ComputeCommerceForBase` in `CommerceRules.h` (17
  test callers each), `EconomyManager::SetEnergyAllocation` (7), `ResourceManager::GetPsych` (5),
  `WorkerAssignmentManager::SetTileScorer` (1), `ProductionManager::Replace` and `Delete` (2)
- **Problem:** no production caller. Queue editing and the energy sliders have no UI yet; the
  commerce functions are reached through `CommerceCache` in production.
- **Why not local:** the tests would move to the production entry points, or the UI arrives.

### Diplomacy, council and atrocity queries used only by tests
- **Where:** `AtrocityLedger::HasVictimized` (6), `HasCommittedMajorAgainst` (5),
  `SanctionUntilYear` (4); `PenaltiesApplied` in `AtrocityMutations.h` (18); `CouncilVoteExecutor::VetoPending` (3),
  `AllMembersVoted` (3); `PlanetaryCouncil::GetSupremeLeaderVictoryStub` (2);
  `GetAvailableActions` in `DiplomacyActions.h` (5); `DiplomacyLedger::AddGrievance` (1),
  `AddIntegrity` (1); `DiplomaticActionExecutor::Accept` (3), `Reject` (1), `GetPendingProposal`
  (4); `MayRepairAt` (4); `TradeKinds` (1)
- **Problem:** no production caller. Most are the halves of diplomacy and council flows whose UI
  or AI is not built; `MayRepairAt` waits on per-turn healing.
- **Why not local:** each waits on its caller, or its tests move to the production path.

## Config parsers and loaders

Rule 7. Each parses a whole file or every kind in one function. The plan: one parser per section or
kind, registered the way the effect types are.

### `EffectConfigParser` — per-kind parsing in one function each
- **Where:** `src/game/effects/EffectConfigParser.cpp`: `ParseCondition` (243 lines),
  `ValidateAmountSourceLegality_` (197), `ParseInteractionOverride_` (142),
  `ValidateEffectForSource` (113), `ParseStatModifier_` (97)
- **Problem:** each walks every kind in one if-chain.
- **Why not local:** the fix is a per-kind dispatch table like the effect-type one, which
  changes how each kind's parser and validator are registered.

### `ProductionConfigParser::ParseConfig`
- **Where:** `src/game/production/ProductionConfigParser.cpp`, 158 lines
- **Problem:** parses every block of production.json inline.
- **Why not local:** over 100 lines; each block's parser fills the same config struct.

### `ParseOccupantArt` and `ParseTileSet_`
- **Where:** `src/game/display/OccupantArtParser.cpp`: `ParseOccupantArt` (107 lines),
  `ParseTileSet_` (86 lines, nesting 6)
- **Problem:** every art shape is parsed inline.
- **Why not local:** over 100 lines; one parser per art shape (variants, tile sets, yield rows)
  would each fill the shared occupant art struct.

### `HotkeyConfig::Load` and `TileRenderer::DrawLinkNetworks_`
- **Where:** `src/ui/HotkeyConfig.cpp` (`Load`, 86 lines, nesting 4), `src/ui/TileRenderer.cpp`
  (`DrawLinkNetworks_`, 93 lines, complexity 28)
- **Problem:** each does several passes inline: binding parse and validation; per-neighbour link,
  replacement and hub drawing.
- **Why not local:** the link drawing shares the neighbour masks across passes, and its art rules
  are SMAC's, so splitting it should be checked against screenshots.

## Variant dispatch

Rule 7. The plan: an exhaustive visitor struct per variant, as `EffectReferenceValidator` uses.

### `ConditionSatisfied` and `ApplyOne_` — one std::visit lambda over every alternative
- **Where:** `src/game/effects/ActiveEffect.cpp` (`ConditionSatisfied(const Condition_t&, …)`,
  173 lines, nesting 24), `src/game/effects/TriggeredEffectDispatch.cpp` (`ApplyOne_`, 189
  lines, nesting 23); `AmountSourceValue` (93) has the same shape
- **Problem:** an `if constexpr` chain inside one lambda handles every alternative.
- **Why not local:** over 100 lines. The fix is an exhaustive visitor struct per variant, as
  `EffectReferenceValidator` now uses, with each arm its own member.

## Long UI and simulation functions

Rule 7. The plan: split each into named steps, checking SMAC ports against the disassembly or
screenshots.

### `WorldView` input and construction
- **Where:** `src/ui/world/WorldView.cpp`: `HandleKey` (146 lines, nesting 11), `HandleMouse`
  (111), the constructor (89, eleven parameters);
  `UnitOrderInputController::HandleKey` (91)
- **Problem:** every key and click case is handled inline.
- **Why not local:** over 100 lines; a per-action dispatch (the hotkey config already names the
  actions) would replace the chains.

### `SocialEngineeringDisplay::Render`
- **Where:** `src/ui/social-engineering/SocialEngineeringDisplay.cpp:327` (164 lines)
- **Problem:** the policy grid, each policy cell and the scores list are laid out inline.
- **Why not local:** over 100 lines; the grid and the scores list share layout values that the
  split must pass between them.

### `moisture_gen::GenerateRainfall`
- **Where:** `src/game/worldgen/MoistureGeneration.cpp`, 281 lines, nesting 9
- **Problem:** the whole Thinker world_rainfall port is one function.
- **Why not local:** over 100 lines; its passes (cloudmass, ocean moisture, rain belts,
  smoothing) share several grids, so splitting it needs a state struct. Confirm each pass
  against the disassembly while splitting.

### `BaseManager::BaseManager`
- **Where:** `src/game/base/BaseManager.cpp`, 52 lines, eight parameters
- **Problem:** the member initializer list builds every subsystem with nested calls.
- **Why not local:** naming the intermediate values needs factory helpers for the subsystems,
  since an initializer list has no statements.

## Duplicated logic

The plan: one shared function each.

### Required-tech availability, in six places
- **Where:** `UnitComponentConfig_t::IsAvailable`, `UnitSlotConfig_t::IsAvailable`,
  `BuildingConfig_t::IsAvailable`, `SocialPolicyConfig_t::IsAvailable`,
  `CouncilProposalConfig_t`, `PopTypeAvailabilityCalculator.cpp:18`
- **Problem:** each repeats "an empty `requiredTech`, or one that is discovered".
- **Why not local:** one shared function serves five domains.

## Single responsibility

Classes that look like more than one responsibility. Record which signals they show:

- after rule 7, the extracted helpers fall into groups that each touch a different subset of
  members;
- describing the class in one sentence needs "and";
- the header includes from unrelated domains;
- methods read none of the members the rest of the class shares.

### `UnitOrderExecutor`
- **Signals:** "and" — it steps, attacks, bombards, founds bases, terraforms and airdrops. The
  `.cpp` includes 43 headers from ten domains. Its private helpers group into movement
  (`TryStep`, `SpendMovesAndEnter_`, `EnterTile_`, the visible-hostile set), combat (`TryAttack`,
  `TryBombard`, `TryIntercept_`, `ResolveScrambleDefender_`, `PromoteOnKill_`) and orders.

### `Unit`
- **Signals:** about 50 public methods in groups that touch disjoint members: cargo
  (`m_pCarrier`, `m_cargo`), supply crawl (`m_tileClaim`, `m_order`), per-turn latches (attack
  history, airdrop, intercept cooldown) and moves and fuel.

## Noted during the abstraction-level pass

Found while applying rules 7 and 8. Each needs a closer look before it changes.

### `UnitOrderExecutor::FightDefender_` — a base check that may be redundant
- **Where:** `src/game/units/UnitOrderExecutor.cpp`, `bDefenderOnBase` in `FightDefender_`
- **Problem:** the attacker only conquers when the defender stood on a base, checked before
  combat. `ResolvePostCombatBaseConquest` already returns nothing without a base, and the
  comment's reason (the tile reference is freed) does not hold, since tiles belong to the map.
  `TryBombard` makes the same call without the check.
- **Why not now:** removing it relies on every `IUnitOrderWorld` doing that check.

### `UnitOrderExecutor::TryAirdrop` — a mechanic decision in an executor
- **Where:** `src/game/units/UnitOrderExecutor.cpp`, `landing.hpAfter < rUnit.GetCurrentHp()`
- **Problem:** the landing only ever lowers hit points. That is a decision `AirdropLanding`
  should make, per `code-kinds.mdc`.
- **Why not now:** it moves the rule into `AirdropLanding` and its tests.

### `PlaceTestImprovementsAroundBases_` — the same nearest-host search twice
- **Where:** `src/game/Engine.cpp`, under `#ifdef AC_PLACE_TEST_IMPROVEMENTS`
- **Problem:** the improvement and farm loops each write out the same nearest-tile search with a
  different filter.
- **Why not now:** the block is compiled out by default, so a change there is not built or run.

### Level-set parsers and `RejectUnknownKeys_` — the same parse in several files
- **Where:** `src/game/DifficultyConfigParser.cpp`, `src/game/natives/NativeLifeLevelConfig.cpp`
  (`default` plus a list of uniquely-id'd levels); `RejectUnknownKeys_` is copied into eight
  parsers (`grep -rl 'void RejectUnknownKeys_' src`)
- **Problem:** each file writes out the same default-and-levels check and its own key whitelist,
  so their messages and rules can drift apart.
- **Why not now:** the shared form belongs in `lib/config` as a template over the level type,
  which changes every caller's error text.
