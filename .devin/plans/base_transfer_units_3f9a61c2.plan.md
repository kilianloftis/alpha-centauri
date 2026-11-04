---
name: Base transfer units
overview: When a base changes hands, the old owner's units at it follow the binary. Mind control and trades convert units through CaptureUnits, trades send the rest home, and every transfer re-homes units the base supported. One rules file decides; one mutations file applies.
todos:
  - id: rules
    content: BaseTransferRules — NearestOtherBase, MindControlGarrison, GivenBaseUnits, LostBaseRehomes
    status: pending
  - id: mutations
    content: BaseTransferMutations — WithdrawUnits, ApplyRehomes, HandOverBase; TakeBase takes the garrison
    status: pending
  - id: callers
    content: Mind control, trades and rebellion call the new paths; TransferCapturedGarrison_ goes
    status: pending
  - id: tests-docs
    content: Tests per rule and per path; deviations, terranx-map and architecture docs
    status: pending
isProject: false
---

# Units at a base that changes hands

Confirmed behaviour: `terranx-map.md` "Base transfer and units". Diplomatic status plays no part
in any of it, and other factions' units are never touched.

| Path | Old owner's units at the base | Units the base supported |
|---|---|---|
| Conquest (`capture_base`) | none (the base was empty) | re-homed to the loser's nearest base; destroyed when it has none |
| Mind control (`probe`, then `capture_base`) | all within one tile, land or water, not on a neighbouring base, join the actor: homed at the base, moves and fuel restored | as conquest |
| Trade (`give_a_base`) | defense and recon units on the tile, and ships, join the receiver; the rest on the tile and on neighbouring land go to the giver's nearest other base, or join when there is none | the binary leaves them homed at the foreign base; we re-home them as conquest does |
| Rebellion (ours) | as trade | as trade |

## 1. Rules: `game/base/BaseTransferRules`

One rules file for what happens to units when a base changes hands. Every function is read while
the old owner still holds the base.

- `const BaseManager* NearestOtherBase(const Faction& rFaction, const BaseManager& rExcluded)`:
  `base_find_2` from `rExcluded`'s tile over `rFaction`'s other bases, by `TabletopDiagonalDistance`,
  the first in `Bases()` order on ties. `nullptr` when there is none.
- `std::vector<CapturedUnit_t> MindControlGarrison(const BaseManager& rBase)`: the owner's units on
  the tile and on neighbouring tiles, land or water, that hold no base (the selection
  `TransferCapturedGarrison_` makes today). Each joins homed at `rBase`, with moves `Refilled` and
  `bRefuels` set. `CapturedUnit_t` gains `bRefuels`; `CaptureUnits` then sets the unit's fuel to
  its maximum.
- `GivenBaseUnits_t GivenBaseUnits(const BaseManager& rBase)`, for a trade or rebellion:
  - selection: the owner's units on the tile, and on neighbouring land tiles;
  - `joining` (`std::vector<CapturedUnit_t>`, homed at `rBase`, moves `Refilled`): a unit on the
    tile whose design plan (`UnitPlanRules::DesignPlan`) is Defense or Recon, every sea unit, and
    every unit when `NearestOtherBase` is `nullptr`;
  - `withdrawn` (`std::vector<WithdrawnUnit_t>`): every other unit, to `NearestOtherBase`'s tile,
    re-homed there when it was homed at `rBase`. Each moves alone, with any cargo it carries
    (`stack_veh`).
  - `WithdrawnUnit_t { Unit* pUnit; const Tile* pDestination; std::optional<BaseId_t> homeBaseId; }`.
- `std::vector<UnitRehome_t> LostBaseRehomes(const BaseManager& rBase, std::span<const Unit* const>
  handled)`: the owner's units homed at `rBase` that `handled` (the garrison, or joining and
  withdrawn) does not name. Each is re-homed to `NearestOtherBase`, or destroyed when that is
  `nullptr` or the unit stands on `rBase`'s tile (`capture_base`).
  - `UnitRehome_t { Unit* pUnit; std::optional<BaseId_t> homeBaseId; }`; unset destroys.

`MindControlGarrison` and `GivenBaseUnits` share one private selection helper over
`ForEachTileInChebyshevRadius(…, 1, …)` (Chebyshev 1 is `vector_dist ≤ 1`); each rule adds its
own neighbour filter.

## 2. Mutations: `game/base/BaseTransferMutations`

- `WithdrawUnits(UnitPositionIndex&, const std::vector<WithdrawnUnit_t>&)`: disembarks a
  passenger, `MoveUnit` to the destination (cargo rides with its carrier, as `ReturnProbeHome_`
  moves a probe), `ClearOrder`, sets the home base when given.
- `ApplyRehomes(GameState&, const std::vector<UnitRehome_t>&)`: sets each home base, or destroys
  the unit.
- `HandOverBase(GameState&, BaseManager& rBase, Faction& rReceiver)`, for trades and rebellion:
  1. `GivenBaseUnits`, then `LostBaseRehomes` over the units it names;
  2. `WithdrawUnits` (before conversion, so a withdrawn passenger leaves a carrier that joins);
  3. `TransferBaseTo` (which drops the Headquarters);
  4. `CaptureUnits(joining)`, then `ApplyRehomes`.
- `TakeBase(GameState&, BaseManager&, Faction& rCaptor, std::vector<CapturedUnit_t> garrison)`
  (`BaseConquestMutations`): `HeadquartersEscapeDestination` and `LostBaseRehomes` over the
  garrison, `NotifyCaptured`, `TransferBaseTo`, `CaptureUnits(garrison)`, `ApplyRehomes`, then the
  escape offer. Conquest passes no garrison.

`CaptureUnits` runs after `TransferBaseTo`, so a home of `rBase` is the new owner's. Each
`LostBaseRehomes` runs before it, since `TransferBaseTo`'s `DropForeignHomeClaims_` clears those
homes.

## 3. Callers

- `ApplyMindControlBase_`: `TakeBase(…, MindControlGarrison(rBase))`. `TransferCapturedGarrison_`
  is deleted. The garrison now gets its home, moves and fuel as the binary gives them.
  `act_of_aggression` already runs before it (the probe aftermath).
- `Deliver_` for `TradeBase_t` (`DiplomaticProposalMutations`) and `PickRebelFactionAndTransfer`:
  `HandOverBase`. Rebellion keeps `NotifyCaptured` before it.
- `ApplyCapture_`: `TakeBase(…, {})`.

## 4. Tests (fixtures only)

- `NearestOtherBase`: tabletop distance, first on ties, the excluded base skipped.
- `MindControlGarrison`: tile and neighbours, not a neighbouring base, another faction's unit left.
- `GivenBaseUnits`: a defense unit on the tile joins; an offense unit on the tile and a unit on
  neighbouring land withdraw; a unit on neighbouring water stays; with no other base all join.
- `LostBaseRehomes`: nearest base, destroyed with none, destroyed on the tile, handled units skipped.
- Paths: a gifted base leaves no giver unit inside it, under every status; a withdrawn carrier
  takes its cargo; a passenger withdrawn off a joining ship stays the giver's; a mind-controlled
  garrison is homed at the base with full moves; conquest re-homes the loser's supported units.

## 5. Docs

- `config/deviations/README.md`:
  - "A given base keeps the giver's units" is removed: the binary's version ships.
  - "Units supported by a lost base go unsupported" becomes "A traded base re-homes the units it
    supported `[Ours]`": the binary leaves them homed at the foreign base. Rebellion is listed
    there as ours too.
  - "Boarded units lose their orders `[Ours]`" also names mind control and trades, which keep
    `ORDER_HOLD` in the binary (`CaptureUnits`).
  - New TODO "A mind-controlled unit's design stays its old owner's": the binary gives the actor
    its own copy of each custom prototype (`propose_proto`); ours keeps the unit on its design and
    adds nothing to the actor's. Missing: the original version.
- `unit-movement-system.md` (conquest, mind control), `diplomacy-system.md` (base trades),
  `population-system.md` (rebellion), and the high-level layout list for the new files.

Units of a third faction stay where they are, as in the binary. One in the new owner's base
without a Pact stays there until that pair's status next changes.
