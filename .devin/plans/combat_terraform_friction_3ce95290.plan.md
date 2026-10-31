---
name: Combat and terraform friction
overview: Write the two remaining binary friction sites that have their prerequisites. Fighting a faction's enemy on its land, while holding an agreement with it, lowers that faction's friction toward the attacker by 1. A human Former's raise/lower land that dries an AI faction's land raises that faction's friction by 5. The +3 refused-demand combat site waits on talk. Relic capture (+5) is already written.
todos:
  - id: re-record
    content: terranx-map.md "Integrity / friction writers" — full conditions for the four battle_fight_2 / action_terraform sites, the 0x800000 demand-refused flag, communicate's inline +4
    status: completed
  - id: config
    content: allied_ground_battle_friction (-1) and terraform_rainfall_friction (5) in shipping + fixture diplomacy.json; parser requires both
    status: completed
  - id: nearest-base
    content: One any-faction nearest-base query (vector_dist, lower BaseId); CaptureRules uses it
    status: completed
  - id: rainfall-changes
    content: RecomputeRainfall / RecomputeClimate / ApplyElevationDelta report each base-moisture change with its before value
    status: completed
  - id: combat-writer
    content: AlliedGroundOwner rule + ApplyAlliedGroundFriction, called first in ApplyFightAftermath_
    status: completed
  - id: terraform-writer
    content: RainfallGrievant rule + ApplyRainfallGrievance, called from ElevationChange_
    status: completed
  - id: docs-deviations-tests
    content: diplomacy-system.md, deviations README entries, fixture tests, ./bd test
    status: completed
isProject: false
---

# Combat and terraform friction

## Context

The standing model's writers are in, except the combat and terraform `cause_friction` sites. These were listed as addresses only. This pass confirmed what each one does in `terranx.exe`
(`sha1` `4b19c1fe3266b5ebc4305cd182ed6e864e3a1c4a`) and writes the ones whose prerequisites
now exist: the rainfall rescan (`2110289f`) and capture (`7024260c`).

| Site | Delta | State |
|---|---|---|
| `battle_fight_2` `0x5078BB` | +3 | Waits on talk (`#DEMANDATTACK`) and a military-strength metric |
| `battle_fight_2` `0x50A1AB` | −1 | **This plan** |
| `battle_fight_2` `0x50B1EF` | +5 | Done: `ArtifactCaptureFriction` |
| `action_terraform` `0x4CA3EB` | +5 | **This plan** |

## Confirmed in the binary

Record these in [terranx-map.md](docs/thinker/terranx-map.md) under "Integrity / friction
writers", replacing the address-only bullets.

**+3, `0x5078BB`.** Runs right after `act_of_aggression(atk, def)` for every non-probe attack on a non-artifact. It then loops over each faction `i` that is neither side and owns the **attacker's** tile (`MAP+7`). It fires when atk→i holds a Pact, def→i has no Vendetta and has a commlink, and `def.mil_strength_1 > i.mil_strength_1` (`+0x1060`). It then calls `treaty_on(i, def, WANT_TO_TALK)`, and only if i→def carries `0x800000` does it call `cause_friction(def, i, 3)`. `0x800000` is set by one thing only: `communicate`'s `treaty_on(ai, player, 0x800000)` (`0x551368`, `0x55172D`, `0x551CF9`) on the refuse branch of `#DEMANDATTACK*`. When the pair holds a Pact, that refusal also adds +4 friction **inline** (`0x551C35`–`0x551C4E`, plus the cached `DiploFriction` at `0x551C65`). That write is outside the 19 `cause_friction` calls. `treaty_on` clears `0x800000` on any status change. Thinker's `mod_battle_fight_2` matches.

**−1, `0x50A1AB`.** Takes the territory owner `o` of the **defender's** tile (`tx, ty`), land only (`alt >= 3`). It fires when `o` is neither side, o→def is at Vendetta and o→atk holds Truce/Treaty/Pact. It then sets `diplo_gifts[atk][o]` to at least 1 (`0x50A19C`) and calls `cause_friction(o, atk, -1)`. It runs before kills and captures (`battle_kill` `0x50A8F4`), whether or not the attacker survives. Bombard strikes never reach it: the strike branch returns at `0x508B4F`. A single round between a sea unit and a non-sea unit (`CT_WEAPON_ONLY`) that leaves both alive also returns first (`0x50A0F8`–`0x50A118`). An unowned tile reads `Faction[-1]` here; Thinker adds a bounds check.

**+5, `0x4CA3EB`.** Only for raise land, lower land and condenser (items `0x10`, `0x11`, `0xC`). Those items all re-run `world_rainfall`, and while `CurrentTurn` is nonzero the scan records a grievant faction in `0x9B22DC` (`0x5C4DE6`–`0x5C4E36`):

- It walks changed tiles in scan order (y, then x) and stops for good once `0x9B22E0` is set. That happens at the first changed tile within 2 of a `CurrentPlayerFaction` base (`base_find_2`, `BaseFindDist <= 2`). The stopping tile is itself still examined.
- For a tile that got **drier** (old > new), when the record is unset or AI, it calls `base_find(x, y)`: nearest base of any faction, no limit, by `vector_dist`, first base index on ties (`jg` at `0x4E3C22`). The record becomes that base's faction. A human record is never replaced.

`action_terraform` then reads the record `v`. It acts only when `v >= 0`, `v` is AI, the terraformer is human, and terraformer→v holds neither Pact nor Vendetta (`test 0x11`). If v→terraformer wants revenge (`0x20`), it calls `set_treaty(v, terraformer, SHALL_BETRAY, 1)`. Otherwise it calls `cause_friction(v, terraformer, 5)`. No commlink is required. Thinker's `action_terraform` rewrite removes both the friction and the betrayal (its TERRAYOURS comment).

## Config

[config/diplomacy.json](config/diplomacy.json) and [tests/fixtures/diplomacy.json](tests/fixtures/diplomacy.json):

- `allied_ground_battle_friction`: `-1` (`ConfigFields::RequireInt`)
- `terraform_rainfall_friction`: `5` (`RequireIntAtLeast` 0)
- `terraform_rainfall_stop_distance`: `2` (`RequireIntAtLeast` 0), the grievant scan's stop radius

Add both to `RejectUnknownKeys` and to `DiplomacyConfig_t` next to `artifactCaptureFriction`.

## One nearest-base query

Add `GameState::FindNearestBase(const Tile&) const`: every faction's bases, by
`TabletopDiagonalDistance` (the binary's `vector_dist`; see `ProbeRules.cpp`), lower `BaseId` on ties. `NearestBaseOfAnyOwner_` in [CaptureRules.cpp](src/game/combat/CaptureRules.cpp) calls it, and its Chebyshev TODO goes. Visible change: a captured unit's home base is now chosen by SMAC distance.

## Rainfall reports what changed

- [MoistureGeneration.h](include/game/worldgen/MoistureGeneration.h): add `struct MoistureChange_t { Tile* pTile; Moisture_t before; };`. `RecomputeRainfall` returns `std::vector<MoistureChange_t>` in tile-index (row-major) order, which is `world_rainfall`'s y-then-x scan. `ChangedSince_` already has the before values.
- `TileEffectsContext::RecomputeClimate()` returns that vector after re-resolving each tile.
- `ApplyElevationDelta` returns `std::optional<std::vector<MoistureChange_t>>`, nullopt when nothing was written. `ApplyEarthquake` and its other callers test `has_value()`.

## Combat writer (−1)

- Rule in [DiplomaticTransitionRules.h](include/game/diplomacy/DiplomaticTransitionRules.h), beside `AttackHostileAct`: `std::optional<FactionId_t> AlliedGroundOwner(FactionId_t attacker, FactionId_t defender, const Tile& rBattleTile)`. It returns the land tile's territory owner (`TerritoryMap::GetOwner`) when that owner is neither side, holds Vendetta with the defender, and holds an agreement (`IsAgreement`) with the attacker. Native life holds neither, so it never qualifies. Add a TODO for the `diplo_gifts` floor (no gifts store yet) and the +3 refused-demand site.
- Mutation in [DiplomaticStandingMutations](include/game/diplomacy/DiplomaticStandingMutations.h): `ApplyAlliedGroundFriction(GameState&, FactionId_t attacker, FactionId_t defender, const Tile&)` → `ApplyFriction(owner, attacker, allied_ground_battle_friction)`.
- Call path: new `IUnitOrderWorld::OnFightResolved(const Unit& rAttacker, const CombatResult_t&, const Tile& rBattleTile)`, implemented by `GameState`. It is called as the **first** step of `UnitAttackExecutor::ApplyFightAftermath_`, before collateral and before post-combat conquest can redraw territory. That covers melee and duels. Strikes and intercept kills never pass through `ApplyFightAftermath_`, matching the binary. Our duels always run until one unit dies, so the single-round skip (where both survive) never applies.

## Terraform writer (+5)

- Rule in [TerraformRules.h](include/game/terraform/TerraformRules.h): `std::optional<FactionId_t> RainfallGrievant(const Faction& rTerraformer, std::span<const MoistureChange_t>)`. It walks the changes in order:
  - A drier tile (`pTile->GetBaseMoisture() < before`), while the record is unset or not player-controlled, sets the record to the owner of `GameState::FindNearestBase(tile)`.
  - After examining a change within 2 (`TabletopDiagonalDistance`) of one of the **terraformer's** bases, it stops. SMAC uses `CurrentPlayerFaction`, which in single player is the human terraformer that the write requires.

  It returns the record only when the record is not player-controlled, the terraformer is, and the pair's status is neither Pact nor Vendetta. Add a TODO: a grievant that wants revenge sets `SHALL_BETRAY` instead (no revenge record).
- Mutation in [TerraformMutations.h](include/game/terraform/TerraformMutations.h): `ApplyRainfallGrievance(GameState&, const Faction& rTerraformer, std::span<const MoistureChange_t>)` → `ApplyFriction(grievant, terraformer, terraform_rainfall_friction)`.
- Call site: `ElevationChange_` in [TriggeredEffectDispatch.cpp](src/game/effects/TriggeredEffectDispatch.cpp), after `ApplyElevationDelta` succeeds, with `rCtx.pUnit->GetFaction()`. `ElevationChange` is used only by `RaiseLand` / `LowerLand` ([terrain.json](config/terrain.json)), so earthquakes and other elevation changes do not write, as in the binary.
- A condenser is still a local overlay with no rescan, so it never dries a tile and never writes.

## Deviations ([config/deviations/README.md](config/deviations/README.md))

- TODO **Wants-revenge terraform betrayal**:
  - Original: a grievant that wants revenge gets `SHALL_BETRAY` instead of +5.
  - Ours: no revenge record, so it always gets +5.
  - Where: `RainfallGrievant`.
  - Missing: revenge record, betrayal flag.
- TODO **Refused-demand ground friction**:
  - Original: the +3 at `0x5078BB`.
  - Ours: not written.
  - Where: TODO at `AlliedGroundOwner`.
  - Missing: talk `#DEMANDATTACK`, the refused flag, a military-strength metric.
- Condenser entry: add to Missing that the original version also carries condenser terraform friction.
- Thinker changes not adopted: **Terraform rainfall friction** ("Integrity / friction writers"). Thinker's `action_terraform` drops the +5 and the betrayal.
- TODO **Allied-ground friction on an unowned tile** `[Bug]` `[Thinker]`: the original reads `Faction[-1]` when the defender's tile has no owner (`0x50A147`, no bounds check). Thinker adds `sq->owner >= 0`. Ours matches Thinker: `AlliedGroundOwner` is nullopt for an unowned tile. Missing: the original is an out-of-bounds read, so it has no playable version.

## Docs

[diplomacy-system.md](docs/architecture/diplomacy-system.md):

- Add `ApplyAlliedGroundFriction` and `ApplyRainfallGrievance` to the "Who writes friction and blemishes?" row.
- In "Not yet built", update the standing bullet: combat −1 and terraform are in. The +3 refused-demand site, talk gifts and the player's excuse dialog are not.

[map-system.md](docs/architecture/map-system.md): `RecomputeClimate` reports base-moisture changes.

## Tests

Bands and deltas come from the fixture. No shipping-config asserts.

- Parser ([DiplomacyConfigParserTests.cpp](tests/faction/DiplomacyConfigParserTests.cpp)): each new key required; `terraform_rainfall_friction` rejects negatives.
- Combat ([DiplomaticStandingMutationsTests.cpp](tests/faction/DiplomaticStandingMutationsTests.cpp), [tests/DiplomacyFixture.h](tests/DiplomacyFixture.h): A player, B AI with base, C AI). A attacks C on B's land, with B at Vendetta with C and B holding a Treaty with A: B→A friction drops by 1, and stays clamped at 0. No write when:
  - B is not at Vendetta with C,
  - B holds no agreement with A,
  - the battle tile is ocean,
  - the owner is the attacker,
  - it is a bombard strike.
  
  An artillery duel writes. The owner is read before a conquest in the same fight.
- `RainfallGrievant` with hand-built `MoistureChange_t` lists:
  - a drier tile nearest an AI base → that AI;
  - nearest a player base → none, and a later AI tile does not replace it;
  - a change near the terraformer's base stops the walk (that tile is still examined);
  - wetter tiles only → none;
  - Pact or Vendetta → none;
  - AI terraformer → none.
- Integration ([TerraformFrictionTests.cpp](tests/game/TerraformFrictionTests.cpp), with the rule tests): a player Former's lower land on a ridge dries the windward belt nearest an AI base and adds the fixture friction.
- Capture: the home-base choice test still passes on `FindNearestBase`. Adjust only cases where the distance metric changes the pick.

## Verification

`./bd test`. Check `LastTest.log` to confirm that the new cases in these files ran: `DiplomacyConfigParserTests`, the combat friction file, `TerraformRules` / `ElevationChangeTests` and `CaptureTests`.
