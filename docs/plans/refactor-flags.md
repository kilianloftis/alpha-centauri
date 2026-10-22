# Refactor flags

Rule violations found during the [readable-code cleanup](readable-code-cleanup.md) that need
more than a local fix. Each one gets a dedicated plan later. Until then the code stays as it is.

One entry per violation, under the rule it breaks:

```markdown
### `Faction::CreateBase` — `bMayOccupyWater`
- **Where:** `include/game/faction/Faction.h:136`; callers in `UnitOrderExecutor.cpp`,
  `Engine.cpp` and tests
- **Problem:** a defaulted flag chooses whether the new base may sit on water.
- **Why not local:** `UnitOrderExecutor` computes it with `FoundedBaseMayOccupyWater(rUnit)`;
  deriving it inside `CreateBase` needs the founding unit, which the other callers do not have.
```

## Rule 1: one call deep

## Rule 3: impossible-case guards

## Rule 4: pass-through functions

## Rule 5: flag parameters

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

## Rule 6: speculative code

### `UnitOrderExecutor` — null `pWorld` and an unset visit handler
- **Where:** `include/game/units/UnitOrderExecutor.h:52` (`pWorld = nullptr`), the `m_pWorld`
  checks in `src/game/units/UnitOrderExecutor.cpp`, `SetImprovementVisitHandler`
- **Problem:** `GameState` always passes itself and sets the handler. Only
  `MoraleCalculatorTests` builds an executor without a world, and only `TriggeredEffectTests`
  clears the handler; the null branches exist for them.
- **Why not local:** those tests need a session, after which the executor can take
  `IUnitOrderWorld&`.

### `UnitManager::IsPendingDestruction` — used only by a test
- **Where:** `include/game/units/UnitManager.h:61`; `tests/game/UnitPositionTests.cpp:128`
- **Problem:** a query whose only caller is a test checking deferred destruction.
- **Why not local:** the test has to assert the behavior through `Units()` and occupancy instead.

### `UnitComponentConfigParser::ParseComponentConfig` — public for tests
- **Where:** `include/game/units/UnitComponentConfigParser.h:21`; called by `ParseConfig` and
  three test files (`ScrapUnitTests`, `CombatRatingTests`, `UnitNamingTests`)
- **Problem:** public only so tests can parse one entry from inline JSON.
- **Why not local:** those tests have to go through `ParseConfig` with a temp file.

### `EventBus::Unsubscribe` and `ScopedConnection::IsConnected` — used only by tests
- **Where:** `include/lib/EventBus.h:29`, `include/lib/Signal.h:73`; `tests/lib/EventBusTests.cpp`,
  `tests/lib/SignalTests.cpp`
- **Problem:** no production caller; the tests check unsubscribe semantics for mod handlers that
  do not exist yet.
- **Why not local:** removing them drops those tests' subject; decide with the mod seams.

## Rule 7: long functions

## Duplicated logic

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
