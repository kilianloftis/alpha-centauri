---
name: FactionGlobal reaches everything the faction owns
overview: FactionGlobal routes like WorldGlobal within its own faction - bases and faction stats as today, plus its live units (unit-domain stats, rule flags) and its units' move cost (tile-domain stats). Social-rating level effects that units and the faction resolve expand once, at the faction's rating. Probe subversion immunity then reads the target faction's flag and TargetHasFlag goes.
todos:
  - id: routing
    content: AppendWorldGlobalEffects becomes AppendGlobalScopeEffects (FactionGlobal and WorldGlobal, domain-matched, rule flags); GatherUnitEffects_, MoveCostRules and TileEffectsContext call it; EffectLane_t comments
    status: completed
  - id: se-expansion
    content: ExpandsAtFactionLevel_ predicate; faction lane appends FactionUnits plus non-Base-domain FactionGlobal level effects at the faction rating; base lane drops the same set; ResolveFlag(Faction) comment
    status: completed
  - id: probe-immunity
    content: CanProbeAction reads ResolveFlag(rTarget.rFaction, ProbeSubversionImmune); TargetHasFlag goes
    status: completed
  - id: tests-docs
    content: Routing, move-cost, SE and baseless-faction immunity tests; effects-system.md scope table, values, SE expansion and unit-collection text; ./bd build and ./bd test
    status: completed
isProject: false
---

# FactionGlobal reaches everything the faction owns

## Scopes

| Scope | Reaches |
| --- | --- |
| `ThisBase` | the base |
| `AllOwnerBases` | every base of the faction |
| `FactionUnits` | every live unit of the faction |
| `FactionGlobal` | the faction and every base (as today), every live unit of the faction (Unit-domain stat modifiers and rule flags) and its units' move cost (Tile-domain stat modifiers) |
| `WorldGlobal` | unchanged: the same, across every faction, plus session tile resolvers |

`FactionGlobal` stays in `EffectLane_t::FactionWide`, so bases and faction resolvers read it as
before. A stat resolved at both levels takes a `FactionGlobal` modifier at both:
`away_from_home_drones` is summed per unit and then scaled at the base, so a `FactionGlobal`
modifier of it applies twice. `AllOwnerBases` and `FactionUnits` pick one level.

Shipping config has no `FactionGlobal` Unit- or Tile-domain stat modifier. Its `FactionGlobal`
rule flags (`allow_gene_warfare`, `inc_gene_warfare_defense`, `orbital_insertion`,
`population_boom`, `probe_subversion_immune`) now also resolve on the faction's units; only
`probe_subversion_immune` is read there.

## Routing ([ActiveEffect.cpp](src/game/effects/ActiveEffect.cpp))

`AppendWorldGlobalEffects(rPool, domain, rOut)` becomes `AppendGlobalScopeEffects` with the same
signature: from `rPool`, every `FactionGlobal` or `WorldGlobal` rule flag, and every
`FactionGlobal` or `WorldGlobal` stat modifier whose `DomainFor` is `domain`.

A faction's composed pool (`Faction::GetActiveEffects()`) holds only its own `FactionGlobal`
effects: peers contribute their `WorldGlobal` ones, and council governor effects reach only the
governor's pool. `CollectSessionWorldEffects()` holds none. So:

- `GatherUnitEffects_` (Unit): a unit takes its own faction's `FactionGlobal` effects.
- `MoveCostRules::Query::EntryTerms` (Tile, the moving unit's faction pool): a faction's
  `FactionGlobal` `move_cost` modifiers apply to its units' moves.
- `TileEffectsContext::CollectAreaEffects` (Tile, session list): unchanged.

[EffectEnums.h](include/game/effects/EffectEnums.h): the `FactionWide` lane comment says
`FactionGlobal` and `WorldGlobal` also reach live units and unit move cost, and `WorldGlobal`
crosses factions and reaches session tile resolvers.

## Social-rating level effects ([SocialRatingResolver.cpp](src/game/social-engineering/SocialRatingResolver.cpp))

A level effect expands at the faction level when units or the faction resolve it:
`ExpandsAtFactionLevel_(const EffectConfig_t&)` is true for a `FactionUnits` effect, and for a
`FactionGlobal` effect that is not a Base-domain stat modifier (rule flags; Unit-, Faction- and
Tile-domain stat modifiers).

- `AppendRatingLevelEffects_` appends every level effect of each axis; its lane parameter goes.
- `ExpandFactionLaneSocialRatingEffects` (faction rating, `FactionWide` modifiers as now) collects
  the level effects and appends to the faction pool those `ExpandsAtFactionLevel_` accepts.
- `ResolveSocialRatingLevelEffects` (the base's own rating) erases those
  `ExpandsAtFactionLevel_` accepts, as it erases `FactionUnits` ones now. Each level effect
  expands once. Bases still see `FactionGlobal` level flags, through the faction pool.

A `FactionGlobal` level flag follows the faction's rating rather than each base's own. No base
modifies Probe locally, so `probe_subversion_immune` resolves as before at every base.

[ActiveEffect.h](include/game/effects/ActiveEffect.h) `ResolveFlag(const Faction&)`: it sees
`FactionGlobal` level flags; `ThisBase` and `AllOwnerBases` level effects expand per base only,
so read those with `ResolveFlag(BaseManager)`.

## Probe subversion immunity ([ProbeRules.cpp](src/game/probes/ProbeRules.cpp))

`CanProbeAction` refuses a paid action when
`ResolveFlag(rTarget.rFaction, RuleFlagId_t::ProbeSubversionImmune)`, the binary's check of the
target faction's rating. `TargetHasFlag` and its declaration go. A faction without bases at SE
Probe +3 now protects its units, which `TargetHasFlag` missed.

## Tests

- [UniversalRoutingTests.cpp](tests/effects/UniversalRoutingTests.cpp): a building's
  `FactionGlobal` `attack` modifier and rule flag reach the owner's live unit and not another
  faction's; `AllOwnerBases` ones reach neither. A `FactionGlobal` `move_cost` `MaxClamp`
  lowers the entry cost for the owner's unit only.
- [RatingTests.cpp](tests/effects/RatingTests.cpp): a faction-wide Probe +3 sets
  `probe_subversion_immune` on the faction, a base and a unit, and the base's list holds it
  once; a `ThisBase` Probe +3 leaves it off.
- [ProbeActionTests.cpp](tests/game/ProbeActionTests.cpp): a baseless faction's unit at SE Probe
  +3 cannot be subverted; the existing immunity test keeps passing.

## Docs

- [effects-system.md](docs/architecture/effects-system.md): the scope table gives
  `FactionGlobal` its own row; the Values list; the `WorldGlobal` row and the
  `CollectLiveUnitEffects` and unit-collection text (lines ~109, ~749, ~1210) name
  `FactionGlobal`; the social-rating expansion bullets describe `ExpandsAtFactionLevel_`; the
  both-levels note for `away_from_home_drones`.

`./bd build` then `./bd test`.
