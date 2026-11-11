# Native Units System

Native life units are not player-composed component assemblies. They use a flat
`NativeDesign` loaded from `config/native_units.json`.

## Design hierarchy

```mermaid
classDiagram
    class IDesign {
        GetId GetName CollectEffects
        GetDomain IsCombatUnit HasComponent
        UsesFuel MaxFuel GetBaseCost
    }
    class UnitDesign {
        slot components
        IConstructable
    }
    class NativeDesign {
        NativeUnitConfig effects domain
        IConstructable
    }
    IDesign <|-- UnitDesign
    IDesign <|-- NativeDesign
    Unit --> IDesign
    Military --> IDesign
```

- `Unit` and `Military` hold `IDesign` (player designs and natives share one ledger).
- `UnitDesign` and `NativeDesign` both implement `IConstructable`. A faction registers every
  native at construction, so they appear in the base build list. The unit designer only lists
  `UnitDesign`.
- `NativeDesign::HasComponent` is always false; the prototype ledger is a no-op for natives.

## Config

[`config/native_units.json`](../../config/native_units.json) — designs only. Each entry: `id`, `name`,
`domain`, `plan` (required; `alpha.txt`'s plan for the predefined unit, read by `IDesign::GetPlan`), `mineral_cost`, `effects[]` (continuous `EffectConfig_t` with
`EffectSourceKind_t::NativeUnit`), optional `on_hold_effects[]`, and optional `planet` (fauna roles / weight / tech gate / lifecycle). Combat natives declare the `native_life` RuleFlag. Alien Artifact omits `planet`.

[`config/native_life.json`](../../config/native_life.json) (+ [`native_life.lua`](../../config/native_life.lua)) — fungal-bloom lifeform count, lifecycle turn thresholds, ambush near-base band, `boat_empty_ocean_min_region_tiles`, abundance `levels`, and Planet fauna formulas. Perihelion scales boat cargo via WorldGlobal `planet_fauna_cargo_scale` on the world event.

`IDesign::IsNativeLife` reads that flag, so a composed `UnitDesign` is native life when any
filled component declares it. Combat natives Add `collateral_damage` 1, the fission-tier
splash they deal without a reactor. When one unit of a wild stack — native life owned by a
`NativeLife` faction — dies in the open, the other native occupants are destroyed. A
faction-owned native takes the numeric splash. Base and Bunker `MaxClamp`
`collateral_susceptibility` to 0, which skips that occupant, including the wild wipe. Combat natives Add `planet_pearls` 10. Killing a wild native — the
defender or a stackmate the fight destroys — pays that base times the intrinsic lifecycle
multiplier from `morale_levels.json` (1 through 7) to the attacker's energy treasury.
A faction-owned native pays nothing.

Shipping natives: Mind Worm, Isle of the Deep, Sea Lurk, Locusts of Chiron, Spore Launcher,
Fungal Tower, Alien Artifact. Fungal Tower is land, movement 0, psi combat, and +50% defense.
It declares `visible_in_fog`: once an observer has explored its tile, `IsUnitVisibleTo`
keeps the live tower drawn and attackable while that tile is fogged. Shroud still hides
it, and concealment still applies. Destroying the tower removes the marker.

## Designs

`Faction` construction registers a `NativeDesign` on `Military` for every entry in
`NativeUnitRegistry`. Production and spawn look it up with `Military::GetDesign`. A fungal
bloom does that on the session's native-life faction (Planet, `config/factions/planet/`) for
each lifeform it spawns. Eco-damage fungal-pop spawning should look the design up the same way.

## Lifecycle starting XP

Centauri Preserve grants `GrantXp` +1 on `on_unit_produced_effects` when `IsNativeLife`
matches. Command Center and Aerospace Complex require `IsNativeLife` `"value": false`
alongside their domain check, so those train bonuses stay on units that are not native life. Rank names
use the `native` column of `morale_levels.json` when `IsNativeLife` is set. A native-life
unit homed at a base pays no mineral support when `UnitSupport` runs if its tile has fungus.
`GetMineralUpkeep` is unchanged, and a zero charge does not take a free support slot.

## Planet lifecycle (wild natives)

Wild natives (`IsWildNative`) use a **live** lifecycle from `NativeRules::WildNativeLifecycleLevel`
for combat/display morale, Isle `IntrinsicXp`, and Planet Pearls — not stored XP alone.
`PlanetFaunaCalculator::LifecycleFloor` maps turn → floor from
`planet_lifecycle_turn_thresholds` in `native_life.json` (vanilla 45/90/170/250/skip/330);
combat promotions above `starting_intrinsic` stack; each design’s `planet.lifecycle_bonus`
(Locusts +1) and `lifecycle` (`turn_floor` / `adjacent_fungus`) apply. See
`docs/thinker/terranx-map.md` `morale_alien`.

## Planet fauna

`WorldEvents` calls `RunPlanetFauna` (`PlanetFaunaMutations`) after terraform spread. Boats,
fungus ambushes, and the near-base swarm use `PlanetFaunaCalculator` (`native_life.lua`),
`PlanetFaunaRules` for gates and candidate picks, `fauna_forms` on native-life levels, and
`planet_fauna_diff` / `planet_fauna_swarm_event` on `DifficultyRules_t`. Designs opt into roles
via `planet.fauna_roles` with a single `fauna_weight`. Boat sites (`AcceptsBoatSpawnSite`):
sea fungus (ocean shelf) or empty ocean whose water region has at least
`boat_empty_ocean_min_region_tiles` tiles — not land fungus. Hull and escort picks require
`CanEnterTileTerrain` for the site; cargo skips the domain filter and embarks onto the hull
(deviation “Planet fauna boats stay on water”). Voice of Planet authors WorldGlobal
`suppress_planet_fauna` on both `effects` and `destroyed_effects`; `PlanetFaunaSuppressed`
reads that flag from faction active effects (owned building pool or tombstone
`destroyed_effects` via session world extras). Fungal-bloom spawn uses designs that declare
`planet`.

## Isle cargo (IntrinsicXp)

Isle of the Deep uses `amount_source: { "kind": "IntrinsicXp" }` on `cargo_capacity` (scale 1) plus
`TransportParams` `carries: [land]`. Live capacity is lifecycle index × amount (wild natives
use `WildNativeLifecycleLevel`; others use `Unit::GetXp()`). Design-only resolve drops the
contribution.

## Fungus movement

Mind Worm and Spore Launcher declare a `move_cost` `MaxClamp` of `"1/3"` when the tile has
Fungus. Isle of the Deep and Sea Lurk declare the same stat with amount `1`. The clamp
ceilings the tile price and cancels the fungus entry rules. On sea fungus, `1` is the
open-sea cost.

## Sea concealment

Sea Lurk and the `Deep_Pressure_Hull` ability share Conceal channel `deep_pressure` gated by
`TargetTileHas Water` (covers Ocean and OceanShelf).

## Alien Artifact link

Alien Artifact declares `on_hold_effects` in [`config/native_units.json`](../../config/native_units.json):
`GrantTech` (random currently-researchable), then `DestroyUnit`, both gated by `BaseHasBuilding`
`Network_Node`. Network Node is an ordinary building (mineral cost 20, upkeep 1). The prompt
fires when the artifact is ordered to Hold in a friendly base that has the node, and when the
node is completed under an artifact that is already Holding there. Linking spends the artifact.
A player design gathers the same list from its components.
