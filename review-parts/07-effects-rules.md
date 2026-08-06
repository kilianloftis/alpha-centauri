## Effects — parsing, tile context, infiltration

**Files:** `src/game/effects/BonusEffectParser.cpp`, `include/game/effects/BonusEffectParser.h`,
`src/game/effects/TileEffectsContext.cpp`, `include/game/effects/TileEffectsContext.h`,
`src/game/effects/TileYieldRulesConfigParser.cpp`, `include/game/effects/TileYieldRulesConfigParser.h`,
`src/game/effects/InfiltrationRules.cpp`, `include/game/effects/InfiltrationRules.h`

**Assessment:** Coverage is good: every effect shape present in `config/` (13 `type` values,
8 scopes, all `op`/condition/selector/filter forms) parses, unknown ids throw with usable
messages, and cross-config id checks live in `EffectReferenceValidator`. Tile yield rules and
infiltration rules are genuinely data-driven — `config/tile_yield_rules.json` carries the caps
and their unlocking techs, and `InfiltrationRules.cpp` contains no game numbers at all. The
dominant weaknesses are in the parser: two required keys are read in a way that aborts instead
of throwing, and `ParseEffectConfig` has grown into a 365-line dispatch that is now the single
place every new rule invariant lands. `TileEffectsContext` is well documented but its three
yield overloads have drifted apart and its resolution pipeline copies its effect list three
times per tile.

### [H] Read required effect keys with `at()`, not `operator[]`
`src/game/effects/BonusEffectParser.cpp:305` — `effectJson["type"]` and `effectJson["scope"]`
use nlohmann's **const** `operator[]`, which does `JSON_ASSERT(it != end())` (json 3.11.3,
`json.hpp:2141`): an effect entry missing `"type"` or `"scope"` aborts in a debug build and
dereferences `end()` in a release build. Everything else in this file reports config errors by
throwing `std::runtime_error` with the offending value, and the file is the load-time gate for
modder-authored JSON, so this is the one input shape that produces a crash instead of a
diagnostic. `ParseEffects` does not pre-check either key, and `ParserTests.cpp` has no case for
a missing `type`. Switch both to `.at()`.

### [H] `ParseEffectConfig` is a 365-line if/else chain carrying all per-type rule invariants
`src/game/effects/BonusEffectParser.cpp:301-666` — one function handles 18 effect types, and
each branch mixes JSON shape parsing with gameplay invariants (scope legality, stat legality,
range checks). The costs are already visible: the "stat must be nutrients/minerals/energy"
predicate is written out three times (`:435`, `:445`, `:463`); scope-vs-type legality is
enforced for `Infiltration`, `TileResourceCap`, `OrbitalAttack` and `TransportParams` but not
for anything else; and `radius` (`:312`) is accepted on any scope even though only
`TileEffectReaches` consumes it and it requires the `TileLocal` lane
(`src/game/effects/ActiveEffect.cpp:88`), so `radius` on a `ThisBase` effect parses clean and
is permanently inert. Every new effect type or invariant extends the same function. Direction:
a `std::unordered_map<std::string, EffectVariant_t(*)(const json&, const EffectConfig_t&)>`
dispatch table with one small function per type, plus shared `IsTileResourceStat_` /
`RequireScope_` helpers.

### [M] Game-balance numbers are silent defaults inside the parser
`src/game/effects/BonusEffectParser.cpp:470` — `cap.max` defaults to `2.0`, i.e. the classic
SMAC pre-tech resource cap, so a `TileResourceCap` that omits `"max"` silently becomes a real
rule nobody wrote. Same shape at `:561` (`chance` 50), `:567` and `:597` (`cooldown_turns` 1)
and `:589` (`chance` 50). This is the only place in the tile-yield path where a game number is
hardcoded — `config/tile_yield_rules.json` otherwise carries the caps and their unlocking techs
in full — and it conflicts with the guideline to prefer throwing over returning defaults.
Require these keys and throw when absent; keep defaults only for genuinely optional switches
(`apply_after_restriction`, `chance_of_destruction_on_fail`).

### [M] Preview yield and worked yield disagree on selector modifiers
`src/game/effects/TileEffectsContext.cpp:287` — `ResolvePreviewTileYield` is
`ResolveTileYield(tile, isBaseTile, baseEffects)` minus the `AppendMatchingTileModifiers_` pass
(`:283`), even though both receive the same `BaseEffects_t`. `BaseWorkableAreaDisplay.cpp:88`
picks the worked overload for worked tiles and the preview overload for unworked ones, so a
"+1 nutrient to every worked Farm" building makes the base screen show 3 on an unworked Farm
that will yield 4 the moment a worker is placed on it — the number the player uses to choose
where to put the worker is the wrong one. `tests/game/TileResourceRestrictionTests.cpp:106-107`
pins this discrepancy as expected, so it reads as deliberate, but nothing states why a preview
should exclude the modifiers the preview is previewing. Fix by giving the preview the same
selector pass (a preview is by definition "as if worked") and collapsing the three overloads
into one whose parameters say what they mean, which also removes the nullable `pCapEffects`
pointer in `ResolveYieldFromEffects_` (`TileEffectsContext.h:87`).

### [M] Unit-projected auras carry no faction attribution
`src/game/effects/TileEffectsContext.cpp:76-98` — improvement auras are stamped with
`ownerFaction` when `ownedByTerritory` is set (`:117-126`) and consumers gate on it via
`AppliesForFaction_` (`:167`, duplicated at `src/game/faction/UnitVisibility.cpp:22`), but
effects collected off units are pushed with `ownerFaction` unset, so they apply to every
faction. The consumer that matters is `HasDetectionCovering_`
(`src/game/faction/UnitVisibility.cpp:64-70`): the first unit component to declare a `ThisTile`
`Detect` will pierce concealment *for the faction being spied on* as readily as for its owner,
and a `ThisTile` `Conceal` will hide enemy units for free. This is latent today — `Carrier_Deck`
is the only component with a `ThisTile` effect and it is a `RuleFlag` read through
`TileProvidesFlag`, which does its own faction check — but the comment at `:76` ("Unit auras are
not territory-owned") records the gap without justifying it. Stamp `ownerFaction` from
`pUnit->GetFaction()` and let consumers keep using the existing filter.

### [M] `ResolveYieldFromEffects_` copies its effect list three times per tile
`src/game/effects/TileEffectsContext.cpp:306` — `std::vector<ActiveEffect_t> filtered = effects`
is an unconditional deep copy (each element owns a `std::string sourceId`) that is left
unmodified whenever no improvement declares `suppressYieldSources`, which is the normal case.
`:337-353` then copies the same elements again into two more vectors, and `ResolveResource_`
(`:293`) runs six times per tile, each call building and sorting a `StatBreakdown_t` whose
`contributions` are discarded — `ApplyModifierStack` (`ActiveEffect.cpp:218`) accumulates into
order-independent sums and a product, so the sort only ever affects display. This partly undoes
the 2026-07-09 lazy-filter work recorded in `docs/code-review-findings.md:44-65`, which removed
exactly this kind of copy from the filter chain. The suppression pass can filter in place while
partitioning (one traversal, no copy of `effects`), and the resolve loop wants a total-only path.
The `(2r+1)²` neighbourhood scan itself is the separate deferred item at
`docs/code-review-findings.md:78` and is not re-reported here.

### [M] `IsCouncilMemberTarget_` invents a membership rule when no council exists
`src/game/effects/InfiltrationRules.cpp:24-29` — with a council present, membership is
`PlanetaryCouncil::IsCouncilMember`, i.e. the explicit member list. With no council, the code
silently substitutes `identity.participatesInCouncil`, which is only the *eligibility* flag the
council constructor validates its members against (`PlanetaryCouncil.cpp:46`) — a strictly
wider set. A `CouncilMembers` filter therefore resolves a different, larger target set before
the council is created than after, with no diagnostic. Either return `false` when there is no
council (no council, no council members) or throw; do not maintain a second membership rule.

### [L] Convention and hygiene items
- `src/game/effects/BonusEffectParser.cpp:137` — `std::stod` accepts trailing garbage
  (`"2abc"` → 2) and throws `std::invalid_argument` without naming the key, unlike every other
  error in the file.
- `src/game/effects/BonusEffectParser.cpp:11-42` — no mapping for `difficult_terrain_cost`,
  although `docs/architecture/effects-system.md:166` lists it as a config-usable stat; the
  enumerator is unreachable from JSON.
- `src/game/effects/BonusEffectParser.cpp:41` — `"council_votes"` return column is misaligned
  with the rest of the table.
- `src/game/effects/BonusEffectParser.cpp:513` — local named `override`; pick a non-keyword name.
- `src/game/effects/BonusEffectParser.cpp:686` — `rContainerJson["effects"]` is iterated without
  an `is_array()` check, so an object-valued `"effects"` silently iterates its members.
- Reference parameters missing the `r` prefix throughout the slice, inconsistently with
  `rContainerJson`/`rStat` in the same files: `BonusEffectParser.cpp:130` (`parameters`, `key`),
  `:152`, `:204`, `:241`, `:282`, `:301`; `TileEffectsContext.cpp:188` (`baseEffects`), `:294`
  and `:303` (`effects`); `TileYieldRulesConfigParser.cpp:12` (`configPath`).
- `include/game/effects/TileEffectsContext.h:51` — boolean parameter `isBaseTile` should be
  `bIsBaseTile`.
- `src/game/effects/TileEffectsContext.cpp:231-247` — `m_maxRadius` takes the max over *all*
  improvement effects but only over `ThisTile` unit-component effects; the two sources should
  use the same rule (only `ThisTile` radii can ever project).
- `src/game/effects/TileEffectsContext.cpp:128` and `:76` — two separate
  `ForEachTileInChebyshevRadius` passes over the identical neighbourhood on every
  `CollectAreaEffects`; they can share one traversal.
- `src/game/effects/TileEffectsContext.cpp:167` — `AppliesForFaction_` is byte-identical to
  `src/game/faction/UnitVisibility.cpp:22`; one of them should be the shared definition.
- `src/game/effects/TileYieldRulesConfigParser.cpp:12` — stateless class with a defaulted
  ctor/dtor and a non-static, non-const `ParseConfig`; it is also the only effects source that
  calls the non-validating `ParseEffects` overload, so `ValidateScopeForSource` never runs on
  `tile_yield_rules.json` (there is no `EffectSourceKind_t` for it).

**Observed outside slice:**
- `src/game/EffectReferenceValidator.cpp:129-205` — every effects source is validated except
  `rData.probeActionsConfig`, so probe-action effect ids are never id-checked.
- `src/game/faction/base/resources/WorkerAssignmentManager.cpp:324-330` — the tile scorer (a full
  `ResolveTileYield`, hence a full neighbourhood scan) is invoked inside `std::sort`'s
  comparator, so each tile is resolved O(log n) times per auto-assign.
- `include/game/effects/ActiveEffect.h:186-226` — `ResolveStatModifiers` always materializes and
  sorts a `StatBreakdown_t`; every hot-path caller that needs only `.total` pays for it.
