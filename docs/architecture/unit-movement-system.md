# Unit Movement System

Movement is split into components with one-way dependencies: rule resolution
(`MoveCostCalculator`), legality checks (`StepEvaluator` + the free functions in
`MovementRules.h` and `TransportRules.h`), planning (`Pathfinder`), and execution
(`UnitOrderExecutor`). All entry-cost and fungus rules live in `MoveCostCalculator`; the
executor and pathfinder only consume its output and never inspect tile features themselves.

```mermaid
graph TD
    subgraph Rules
        MovementRules[MovementRules<br/>terrain domain entry, unaided occupancy,<br/>ZOC, friendly occupant / base, stacking]
        TransportRules[TransportRules<br/>cargo domains / capacity / load sites,<br/>boarding-aware CanEnterTile, CanUnloadTo]
        MoveCostCalculator[MoveCostCalculator<br/>tile costs + fungus entry rules]
        EntryTerms[EntryTerms_t<br/>costFragments,<br/>bRequiresFullCost, bEndsTurn]
        MoveCostCalculator -->|resolves per unit + tile| EntryTerms
        TransportRules --> MovementRules
    end

    subgraph Conquest
        BaseConquestRules[BaseConquestRules<br/>pure predicates: garrison, amphibious<br/>assault, capture veto, species]
        BaseConquestEffects[BaseConquestEffects<br/>world mutation: pop loss, facility<br/>destruction, capture, raze, raid]
        BaseConquestConfig[BaseConquestConfig_t<br/>config/base_conquest.json]
        BaseConquestEffects --> BaseConquestRules
        BaseConquestEffects --> BaseConquestConfig
    end

    StepEvaluator[StepEvaluator<br/>edge legality: adjacency, terrain,<br/>occupants, ZOC — objective or<br/>faction-known knowledge]
    Pathfinder[Pathfinder<br/>Dijkstra over PlannedCostFragments<br/>+ CanPlanStep]
    UnitOrderExecutor[UnitOrderExecutor<br/>TryStep / order loop,<br/>spends fragments, banks charges]
    IUnitOrderWorld[IUnitOrderWorld<br/>session surface: FindBaseAt,<br/>intercept, conquest]
    GameState[GameState]

    StepEvaluator --> MovementRules
    StepEvaluator --> TransportRules
    MoveCostCalculator --> MovementRules
    Pathfinder --> StepEvaluator
    Pathfinder -->|PlannedCostFragments| MoveCostCalculator
    UnitOrderExecutor -->|EntryTerms| MoveCostCalculator
    UnitOrderExecutor --> StepEvaluator
    UnitOrderExecutor --> Pathfinder
    UnitOrderExecutor --> TransportRules
    UnitOrderExecutor --> BaseConquestRules
    UnitOrderExecutor -->|optional, ctor-injected| IUnitOrderWorld
    GameState -.->|implements| IUnitOrderWorld
    IUnitOrderWorld --> BaseConquestEffects
```

## MoveCostCalculator — the single home of entry rules

`ForUnit(unit, map)` returns a `Query` that caches the unit's rule flags
(`IgnoreDifficultTerrain`, `TreatFungusAsRoad`). A `Query` resolves each tile into an
`EntryTerms_t`:

- **`costFragments`** — the tile's entry price. Highest `move_cost` among the tile's terrain
  features and improvements; any `move_cost_override` replaces that result entirely (even
  when higher), and among multiple overrides the lowest wins (MagTube 0 beats Road 1/3).
  Nothing configured → `defaultMoveCost`. `IgnoreDifficultTerrain` caps non-fungus feature
  costs at the default. `TreatFungusAsRoad` makes fungus contribute Road's
  `move_cost_override` instead of its own cost.
- **`bRequiresFullCost`** — the full price must be banked (possibly across turns) before the
  unit may enter. Set for fungus without an override in play and without a friendly occupant.
  When false, any positive fragment balance admits the unit (the cost clamps to what
  remains) — the default terrain rule.
- **`bEndsTurn`** — entering zeroes the unit's remaining fragments. Set for fungus without
  an override in play, friendly occupant or not.

An override "in play" (a Road or MagTube built on the tile, or `TreatFungusAsRoad`) negates
both fungus flags along with the cost — infrastructure bypasses the fungus entry rules.

Costs are integer *fragments*: `MovementConstants_t::k_moveFragmentsPerPoint` (360) per
movement point, so fractional configs like Road's `"1/3"` stay exact integers.

## Planning vs execution

The `Query` exposes two views of the same terms, one per consumer:

- `EntryTerms(tile)` — the raw rules, resolved from objective tile state. Consumed by
  `UnitOrderExecutor` when actually spending fragments.
- `PlannedCostFragments(tile)` — Dijkstra edge weight for `Pathfinder`. Shrouded
  (unexplored) tiles report the default cost so the planner cannot see rockiness / fungus /
  roads under fog. End-turn entries are valued in whole turns of the unit's movement
  allotment: a banked entry costs `ceil(cost / allotment)` turns, an immediate
  (friendly-occupant) entry exactly one — this is why a clear detour beats a
  "cheaper-looking" fungus shortcut.

## Execution

`UnitOrderExecutor::SpendMovesAndEnter_` is rule-agnostic — it just acts on the terms:

1. `bRequiresFullCost` and the banked total (`MoveOrder_t::chargeFragmentsPaid` toward
   `pChargeTile`) plus this turn's fragments still fall short → bank the remainder, zero the
   unit's fragments, stay put. Switching charge target resets the bank.
2. Otherwise enter: remaining fragments become `0` when `bEndsTurn`, else
   `available - costFragments` (clamped at 0, so ordinary terrain admits a last-fragment
   entry into any cost).

`Execute_(MoveOrder_t)` loops `Pathfinder::NextStep` → `TryStep`, re-planning after every
step because each step can reveal fog or hostiles (which also cancels the order via
`CancelMoveOrderIfNewHostile_`).

`SpendMovesAndEnter_` splits arrival into two phases. `EnterTile_` does position and move
cost only; `ApplyArrivalEffects_` then runs the side effects of *being* on the new tile —
boarding a transport parked there, and base-entry conquest.

### A step can destroy the mover

`TryStep` returns `StepResult_t { bEntered, bMoverDestroyed }` rather than a bool, because a
native-life raider is consumed by the base it raids: the step legally succeeds and leaves no
mover behind. The flag propagates outward — `ApplyArrivalEffects_` → `SpendMovesAndEnter_` →
`TryStep` → `Execute_` → `Execute` → `PlayerActions` — and every layer stops touching the
unit once it is set. `Execute` reports this as `OrderProgress_t::UnitDestroyed`, which is
distinct from `Expended`: `Expended` means *the caller must* `DestroyUnit`, `UnitDestroyed`
means it already happened. `TryStep` is `[[nodiscard]]` so this cannot be dropped silently.

## Transports and cargo

`TransportRules` sits above `MovementRules` and uses it; `MovementRules` knows nothing about
cargo, which keeps the dependency one-way. Three entry predicates form a ladder:

- `CanEnterTileTerrain` (MovementRules) — chassis domain against raw terrain.
- `CanOccupyTileUnaided` (MovementRules) — the above, plus a land unit garrisoning a friendly
  sea base. "Can this unit hold this tile with nothing under it?"
- `CanEnterTile` (TransportRules) — the above, plus the land exceptions that depend on what
  else is on the tile: boarding a friendly transport, or an Amphibious assault onto a
  sea-base tile. Neither grants free ocean movement.

Carrier capability is entirely config-driven via the `TransportParams` effect: which
passenger domains a carrier accepts, and which tile capabilities it needs in order to
exchange cargo (`loadSiteFlags`, resolved through `TileProvidesFlag` — see
`effects-system.md`). Capacity itself is the `cargo_capacity` stat.

An embarked unit shares its carrier's tile but is excluded from ZOC projection, combat
targeting, tile occupancy, and visible-hostile scans; `Unit::IsEmbarked` is the single gate.
`UnitPositionIndex::MoveUnit` carries cargo along with the carrier, and `StepEvaluator`
routes an embarked mover through `CanUnloadTo` instead of the normal terrain check.

### Boarding is only automatic where it has to be

Two entry points, deliberately different:

- `TryAttachToTransport` — the explicit **L** order. Boards wherever boarding is legal,
  including inside a base.
- `TryAutoAttachOnEntry` — applied silently by `ApplyArrivalEffects_` after a step. Boards
  **only** when the passenger fails `CanOccupyTileUnaided`, i.e. when it could not otherwise
  be on that tile at all.

So stepping onto open water loads a land unit onto the transport waiting there, while walking
into a base never stows a unit behind the player's back. `SurvivesCarrierLoss` uses the same
predicate from the other direction: when a carrier is destroyed, cargo that can hold the tile
unaided is set down on it and cargo that cannot goes down with the carrier — a ship sunk in
port does not drown the garrison, one sunk at sea does.

`HasBaseGarrison` is the one place that deliberately counts embarked units. Cargo parked in a
base still holds it against capture, which is consistent with surviving the carrier's loss
there. It does not make the cargo fight: `FindVisibleHostileOnTile` still skips embarked
units, so the carrier remains the only thing an attacker can target.

## Base conquest

Conquest is split so the predicates stay testable without a live world:

- `BaseConquestRules` — pure predicates only. `HasBaseGarrison`, `CanAttackIntoBaseTile`
  (land needs Amphibious against a sea-base garrison), `CanCaptureBase` (`CannotCaptureBases`
  is the sole veto — Needlejet/Missile chassis, noncombat modules), and the species tests.
- `BaseConquestEffects` — every world mutation, returning a `BaseConquestResult_t` tally
  (population lost, facilities destroyed, escape pods, razed, actor destroyed).

Two entry points, reached from `UnitOrderExecutor`:

- `ResolvePostCombatBaseConquest` — after the last garrison on a base tile dies. Applies
  last-defender population loss (unless the base has `PreventsConquestPopLoss`), then a
  native raid if the attacker is native life. **Capture requires stepping onto the tile**, so
  combat alone never transfers ownership.
- `ResolveBaseEntryConquest` — a unit entered an ungarrisoned foreign base tile. Native life
  raids; anything else captures if `CanCaptureBase`.

Razing (population reaching zero) tombstones the base's Secret Projects through
`GameState::MarkSecretProjectDestroyed`, so no faction can rebuild them.

Both entry points need session-wide state the map and pathfinder cannot supply. That reaches
the executor through `IUnitOrderWorld`, a narrow interface `GameState` implements and passes
to the executor's constructor. It is nullable: movement-only test harnesses build an executor
with no world, which disables intercept and conquest but leaves stepping intact.
