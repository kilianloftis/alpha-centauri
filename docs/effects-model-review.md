# Effects model code review

**Date:** 2026-08-04
**Extracted from:** [`docs/full-code-review.md`](full-code-review.md) (August 2026 full-project review)
**Scope:** Effect vocabulary and resolution (`ActiveEffect`, `BonusEffect`, `EffectEnums`, `DeployCooldown`), parsing (`BonusEffectParser`, tile-yield rules), tile/infiltration runtime (`TileEffectsContext`, `InfiltrationRules`), effect pools / providers, and load-time effect reference validation — plus cross-slice findings whose fix is about those contracts.

## Summary

| Severity | Count |
|----------|------:|
| High `[H]` | 9 |
| Medium `[M]` | 25 |
| Low `[L]` | 2 |
| **Total** | **36** |

## Contents

- [Effects — core model and vocabulary](#effects-core-model-and-vocabulary)
- [Effects — parsing, tile context, infiltration](#effects-parsing-tile-context-infiltration)
- [Cross-cutting findings](#cross-cutting-findings)

---

## Effects — core model and vocabulary

**Files:** `src/game/effects/ActiveEffect.cpp`, `include/game/effects/ActiveEffect.h`,
`include/game/effects/BonusEffect.h`, `include/game/effects/EffectEnums.h`,
`include/game/effects/DeployCooldown.h`

**Assessment:** This is the strongest-designed subsystem I have read in this repo. The
scope→lane routing (`LaneFor`) and seed semantics (`KindFor`/`SeedFor`) are genuine
compiler-enforced single sources of truth, the `FactionEffects_t`/`BaseEffects_t` split turns
a pipeline-stage mistake into a compile error, and every prior-review item I checked
(`ApplyModifierStack` extraction, `Instantaneous` gating, faction-wide grant attribution,
`TileYieldModifierEffect_t` removal) is genuinely resolved. The dominant weaknesses are at
the edges of that discipline: two arithmetic conventions that were never centralised
(int rounding), one gap in grant deduplication, and a family of "kind + bag of optionals"
config structs whose validity is asserted in comments rather than in types.

### [H] Grant expansion double-counts a building the base already constructed
`src/game/effects/ActiveEffect.cpp:141` — `processedGrantedIds` is seeded empty and only
records grant *expansions*, so it dedupes grant-vs-grant (pinned by
`tests/effects/GrantExpansionTests.cpp:95`) but not grant-vs-constructed. The input vector
already contains every constructed building's effects (`FactionEffectsPool::CollectBuildingEffects_`
concatenates `BaseManager::CollectBuildingEffects` for all bases), so a base that has built
Perimeter Defense *and* owns a project granting it gets the bonus twice — the canonical SMAC
Command Nexus case, where the granted copy is supposed to be wasted. `GrantChainContains_`
does not catch it: it only inspects the grant's own source chain. No shipped config declares
a `GrantBuilding` yet (only `config/buildings/README.md:158`), so this is latent, but the
dedup logic is implemented and incomplete rather than absent. Fix: pre-seed
`processedGrantedIds` with `{originBase, sourceId}` for every effect already in the input.

### [H] Two different int-rounding policies for the same stat
`src/game/effects/ActiveEffect.cpp:475`, `:523`, `:529` — every `ResolveStat` overload ends in
`static_cast<int>(...)`, truncating toward zero, while `MoraleCalculator::ResolveCombatStat`
(`src/game/units/MoraleCalculator.cpp:233`) and `PlanetaryCouncil` (`PlanetaryCouncil.cpp:175`)
round with `std::lround` over the same `ApplyModifierStack` output. Shipped data reaches both
paths: `config/unit_components/chassis.json:19` is a `+25% AddPercent` attack modifier, so a
strength-2 attacker resolves to 2 through `Unit::GetStat(Attack, ctx)` and to 3 in combat, and
`config/unit_components/specials.json:69` (`movement -50%` for air units) makes
`Unit::GetMovementPoints()` truncate any odd movement total downward, with no stated rule
that it should round down rather than up. The rounding convention is core
vocabulary and belongs in one place — pick it here (in the `ResolveStat` family) and have
combat call the same helper, rather than leaving each consumer to choose.

### [M] The lazy filters silently accept rvalue containers
`include/game/effects/ActiveEffect.h:236`, `:255`, `:276`, `:298` — all four filters take
`const std::vector<ActiveEffect_t>&` and return a view that borrows it. The contract is
documented four times ("consume it within the statement…"), and the hazard has already
required three hand-written workarounds in this file alone
(`ActiveEffect.cpp:369-372` in `CollectFromPops`, the "Materialize first" comment at `:488`,
and the named locals at `:522`/`:528`). Two live call sites still pass a prvalue directly
(`ActiveEffect.cpp:476` and `:482`, `FilterByStatId(rDesign.CollectEffects(), …)`); those are
safe only because the whole chain is one full-expression — the same expression in a range-for
dangles unless the toolchain implements P2718R0. A deleted rvalue overload
(`auto FilterByStatId(std::vector<ActiveEffect_t>&&, StatId_t) = delete;`) converts every such
trap into a compile error and lets the four repeated comments go away.

### [M] `DispatchInstantaneousEffects`'s optional `GameState` silently drops Infiltration
`include/game/effects/ActiveEffect.h:384` declares `GameState* pGameState = nullptr`, and the
only production caller (`src/game/faction/base/BaseManager.cpp:113`) omits it — so an
Instantaneous `Infiltration` on a constructed building always takes the
`ActiveEffect.cpp:430-435` branch, prints a TODO to stderr, and drops the effect. `Infiltration`
is not hypothetical (`config/council/rules.json:12`, `config/probe_actions.json:16`), so the
first building that declares one will silently no-op. The default argument is what makes the
incomplete wiring compile; per the project's "throw on unexpected null" rule this parameter
should be a `GameState&` and `BaseManager` should be required to supply it.

### [M] Origin tagging has a second, hand-maintained implementation
`src/game/effects/ActiveEffect.cpp:57-61` derives `bTagOrigin` from `LaneFor`, and the header
(`ActiveEffect.h:118-120`) states this is "the single config->ActiveEffect_t conversion".
It is not: `BuildingManager::CollectEffects` passes `nullptr` as the origin and
`BaseManager::CollectBuildingEffects` (`src/game/faction/base/BaseManager.cpp:233-242`)
re-tags afterwards using a hand-written three-scope list. Adding a scope updates `LaneFor`
and the compiler stays silent about the copy, so building effects would quietly lose their
origin. Export the predicate from this header (e.g. `constexpr bool TagsOriginBase(EffectScope_t)`
next to `LaneFor`) so the second site cannot drift — or let buildings pass their base through
`AppendActiveEffects` and delete the re-tag loop.

### [M] `Condition_t` / `UnitFilter_t` / `TileSelector_t` are kind-tagged bags of optionals
`include/game/effects/BonusEffect.h:333-341`, `:355-364`, `:154-158` — each carries a `kind`
plus every possible parameter, with comments saying which field is live for which kind and
nothing enforcing it. The consequences are concrete: `UnitFilterSatisfied`
(`ActiveEffect.cpp:296-308`) returns **false** when the optional for the declared kind is
absent, which silently drops the effect from every unit rather than reporting a malformed
config; and `Condition_t::values` (an implicit AND of `TargetTileHas`) duplicates what
`Condition_t::conditions` already expresses, so `AllOf` has two encodings for the same rule
and `ConditionBodySatisfied_` has to evaluate both, ending in a `!values.empty() ||
!conditions.empty()` guard (`ActiveEffect.cpp:272`) whose purpose is only to reject an empty
node. The file already models sum types correctly with `EffectVariant_t`; these three should
use the same tool (`std::variant<TargetTileHas_t, AllOf_t, …>`), which removes the invalid
states instead of documenting them.

### [M] Effect structs carry balance defaults that the parser also carries
`include/game/effects/BonusEffect.h:239` (`chance = 50`), `:242` (`cooldownTurns = 1`),
`:188` (`TileResourceCapEffect_t::max = 2`) each duplicate the default already passed by
`BonusEffectParser.cpp:561`, `:567`, `:470`. Two sources of truth for a balance number, and
the in-struct one is what a future non-JSON construction site would silently get. These are
game rules, so they belong in config: drop the member initialisers and let the parser be the
one place that decides what an omitted field means (ideally by throwing).

Related, in the same header: `EffectConfig_t::scope` and `::persistence`
(`BonusEffect.h:386-387`) have no default member initialiser, unlike every neighbouring
struct. A `EffectConfig_t cfg;` (the pattern used in `tests/effects/ValidationTests.cpp:24`)
leaves both indeterminate, and `AppendActiveEffectsIf_` reads `persistence` first thing.
`RuleFlagEffect_t::flag` (`:193`), `SocialRatingModifierEffect_t::rating` (`:208`),
`DiplomaticModifierEffect_t::value` (`:216`), and the three `kind` fields above have the same
gap.

### [M] `EffectiveStatModifierAmount` returns 0.0 for a missing context
`include/game/effects/ActiveEffect.h:96-101` — an `amountSource` modifier resolved without a
`targetTile` contributes 0.0. For `Add` that is the pinned behaviour
(`tests/effects/ModifierMathTests.cpp:222`) and defensible, but the parser only restricts
`amount_source` to `stat: energy` + `scope: ThisTile` (`BonusEffectParser.cpp:413-423`), not
to `op: Add`. A `MultiplyGeometric` amount_source modifier resolved without a tile therefore
multiplies the whole stat by zero rather than being skipped. Either reject non-`Add` ops for
`amount_source` at parse time, or make the "no context" case skip the contribution rather
than fabricate a neutral-looking number — a silent 0.0 is only neutral for one of the three
ops.

### [M] Four copies of the same flag scan, and a null check nothing can trigger
`src/game/effects/ActiveEffect.cpp:503`, `:551`, `:568`, `:585` — the four `ResolveFlag`
overloads have byte-identical bodies over different ranges; only the range expression
differs. They should be one range-templated helper (the file already does exactly this for
`ResolveStatModifiers`). Each of them, plus ten other sites, also opens with
`if (!rEffect.config) continue;` — but `ActiveEffect_t` is only ever produced by
`AppendActiveEffectsIf_`, which always assigns `&rEffect`, so no production path can create a
null config. The invariant is real but only enforced by convention, and every consumer pays
for it; giving `ActiveEffect_t` a constructor taking `const EffectConfig_t&` would make it
structural. Note `tests/effects/ModifierMathTests.cpp:180` currently pins the null-tolerant
behaviour, so that test encodes the weaker requirement and would need to change with it.

### [M] `HasPermission` re-filters what the collector already removed
`src/game/effects/ActiveEffect.cpp:616` calls `UnitFilterSatisfied` on effects returned by
`CollectLiveUnitEffects`, which erased every non-matching effect at `:453-469`. Either the
collector's guarantee is real and this check is dead, or the guarantee is not something
callers can rely on — in which case `ResolveStat`/`ResolveFlag`, which do *not* re-check, are
wrong. Pick one and state it on `CollectLiveUnitEffects`.

### [L] Convention and hygiene items
- `include/game/effects/BonusEffect.h` is named after a type that does not exist, and holds
  the majority of the effect enums (`EffectScope_t`, `EffectLane_t`, `ModifierOp_t`,
  `ConditionKind_t`, `PermissionId_t`, …) while `EffectEnums.h` holds only `StatId_t`,
  `SocialRatingId_t` and `RuleFlagId_t` — the two file names invert their contents.
- `include/game/effects/ActiveEffect.h:32`, `:36`, `:80` — `config`, `originBase` and
  `targetTile` are pointers without the required `p` prefix.
- `include/game/effects/ActiveEffect.h:160` — nested data struct `Contribution` is missing the
  `_t` suffix that `StatBreakdown_t` itself carries.
- `src/game/effects/ActiveEffect.cpp:235` — `CollectActiveEffects` is a one-line forwarder to
  `IEffectsProvider::GetActiveEffects()`; callers already hold the provider.
- `include/game/effects/ActiveEffect.h:174` — "Contributions are applied in the order given"
  is misleading; `ApplyModifierStack` partitions by op, and
  `tests/effects/ModifierMathTests.cpp:85` pins order-independence.
- `src/game/effects/ActiveEffect.cpp:111-130` — the grant cycle guard parses the `" -> "`
  separator out of the user-facing breakdown label, coupling cycle detection to a UI string
  format; a parallel `std::vector<std::string>` chain on the expansion frame would not.
- `include/game/effects/ActiveEffect.h:40` — `ownerFaction`'s gate
  (`!has_value() || *v == faction`) is re-implemented at `src/game/faction/UnitVisibility.cpp:24`
  and `src/game/effects/TileEffectsContext.cpp:170`; it belongs next to the field.
- `include/game/effects/EffectEnums.h` — the snake_case wire maps for `StatId_t`,
  `RuleFlagId_t` and `SocialRatingId_t` live in `BonusEffectParser.cpp` rather than "next to
  the enum" as the guidelines require; there is only one map each, so this is placement only.
- `tests/effects/ValidationTests.cpp:59-87` claims to pin every stat's `KindFor`, but
  `PsiDamage` and `TechCost` are missing — exactly the compile-time gap the pin exists to close.
- `include/game/effects/DeployCooldown.h` is clean: one formula, one place, used by both
  `OrbitalAttack.cpp:115` and `InterceptRules.cpp:138`.

**Observed outside slice:**
- `src/game/effects/BonusEffectParser.cpp:92-127` — `ParseModifierOp`, `ParseEffectScope`,
  `ParseEffectPersistence` and `ParseAmountSource` hand-roll if-chains whose wire form is
  exactly the enumerator name; the guidelines mandate `magic_enum` here (it is already a
  dependency and included at the top of that file).
- `docs/architecture/effects-system.md:138` describes `radius` as Manhattan distance; the code
  (`ForEachTileInChebyshevRadius`, `TileEffectsContext.cpp:80`) and `BonusEffect.h:402` both
  say Chebyshev.
- `src/game/units/UnitDesign.cpp:122` works around seed semantics with
  `contributions.empty() ? 1.0f : total` instead of trusting `SeedFor(CostMultiplier) == 1.0`,
  which is the same value — dead branch hiding the documented rule.

---

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

---

---

## Cross-cutting findings

Findings originally filed under other subsystems that concern the effects model (collection, expansion, attribution, validation, or resolution contracts). Source slice noted on each item.


### From: Base management — BaseManager, home-base index, buildings

### [H] Two divergent effect lists per base: the memo omits world and council effects
`src/game/faction/base/BaseManager.cpp:328` and `:343` — `ProduceResources` / `ApplyGrowth`
take a caller-supplied pool and build the base list fresh, while `ApplyProduction` (`:266`),
`GetMineralCost` (`:271`), the five production getters (`:194`–`:217`),
`GetNutrientsRequired` (`:348`) and `GetWorkedTileYield` (`:368`) use the memoized
`BuildBaseEffects_()` (`:300`) over `m_pEffectsProvider`'s pool alone. The supplied pool is
strictly larger: `Faction::ProduceBaseResources` / `ApplyBaseGrowth` (`src/game/Faction.cpp:455`,
`:469`) append `GameState::CollectWorldEffects` — other factions' `WorldGlobal` effects plus
the council's world laws and the governor's faction-global effects
(`src/game/GameState.cpp:130`–`140`). Concretely: `ResourceCollection` banks minerals
resolved *with* council effects while `BaseProduction` charges a mineral cost resolved
*without* them (`src/game/stages/BaseProduction.cpp:33`); `PopulationManager::ApplyGrowth`
spends a growth threshold computed with them
(`src/game/faction/base/population/PopulationManager.cpp:169`) while `GrowthDisplay` shows
the threshold from `GetNutrientsRequired()` computed without them. Same base, same turn,
two answers. Fix direction: make the provider's pool the only pool (fold world/council
contributions into `IEffectsProvider`) and delete the `FactionEffects_t`-taking overloads,
so `BuildBaseEffects_()` is the single entry point.

The memo itself is correctly *invalidated*, for what it covers: the key is the pool version,
and `FactionEffectsPool::CollectRevisions_` includes every per-base building and population
revision (`src/game/faction/FactionEffectsPool.cpp:117`–`129`), which are exactly the local
inputs of `CollectBaseLocalEffects_`. The bug is what never enters the key, not stale keying.

### [M] `CollectBuildingEffects` re-implements the origin-tagging rule that the effects layer owns
`src/game/faction/base/BaseManager.cpp:233`–`241` stamps `originBase` for a hardcoded triple
(`ThisBase || ProducedAtThisBase || FactionUnits`), which is a copy of the rule already
implemented in terms of `LaneFor` at `src/game/effects/ActiveEffect.cpp:57`–`61`.
`BuildingManager::CollectEffects` passes `nullptr` for the origin
(`BuildingManager.cpp:63`) purely so this post-pass exists. Adding a new base-lane scope
updates `LaneFor` and silently misses `BaseManager`'s copy. Fix: let
`BuildingManager::CollectEffects` take the owning base and pass it to `AppendActiveEffects`,
then delete the loop.

### [M] Instantaneous `Infiltration` is unreachable from the only production-completion path
`src/game/faction/base/BaseManager.cpp:113` calls `DispatchInstantaneousEffects(building, *this)`
with no `GameState`, and `BaseManager` has no route to one. The infiltration branch therefore
prints a `[TODO]` line to stderr and drops the effect
(`src/game/effects/ActiveEffect.cpp:428`–`437`) for every building a base ever completes —
this is the *only* production dispatch site in the game. No shipped building declares one yet,
so nothing is wrong today, but a modder who writes one gets a stderr note and no effect
rather than a rejected config. Fix direction: give the dispatch site the session it needs
(inject the world surface the base already needs for conquest, or route completion through a
handler that has `GameState`), or reject the effect kind at config validation.


### From: Base management — resources and worker assignment

### [H] Base-level percentage modifiers are silently discarded
`src/game/faction/base/resources/ResourceManager.cpp:124-126` resolves base-level stat modifiers
against a **zero** seed and then adds the result to the worked total:

```cpp
double base = static_cast<double>(GetResourceValue_(worked, stat));
base += ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, stat), SeedFor(stat)).total;
```

`ApplyModifierStack` computes `(seed + adds) * arithmeticFactor` (`src/game/effects/ActiveEffect.cpp:218-233`),
so with `SeedFor(Nutrients) == 0.0` an `AddPercent` contribution multiplies zero and vanishes.
This is not hypothetical: `config/social_rating_effects.json:19,23` gives the Economy rating levels
2/3 an `AllOwnerBases`, selector-free `minerals -10% / -20%` penalty, and
`TileEffectsContext::AppendMatchingTileModifiers_` deliberately forwards only *selector-carrying*
modifiers to the per-tile pass (`src/game/effects/TileEffectsContext.cpp:186-204`) — so that penalty
is applied nowhere in the game. `CalculateEcon_/Labs_/Psych_` (lines 145, 153, 161) have the same
shape, so any future "+50% labs at this base" facility will also be a no-op. Fix: seed the resolve
with the value being modified (`ResolveStatModifiers(filter, static_cast<double>(worked))`), exactly
as `Pop::ApplyTileMultipliers` already does (`src/game/population/pop-types/Pop.cpp:110-111`).


### From: Faction — economy, research, social engineering, identity

### [H] Research cost cache is keyed on the effects version but also depends on tech count
`src/game/faction/ResearchManager.cpp:116-118,131-141` — `ComputePointsNeeded_` feeds
`m_discoveredTechs.size()` into `TechCostInputs_t::techs`/`mostTechs`, but
`RevalidatePointsNeeded_` only compares `m_pEffectsProvider->GetEffectsVersion()` against
`m_costEffectsVersion`, and returns early when there is no provider. `AddDiscoveredTech` is
called mid-research from three production paths that never touch the target
(`src/game/units/ProbeActionEffects.cpp:72` tech steal,
`src/game/faction/DiplomaticActionExecutor.cpp:319` treaty tech, `Engine.cpp:322`); each
changes a direct input to the cost. Today it happens to work because
`FactionEffectsPool::CollectRevisions_` stamps the research revision, so the pool version
moves and drags the cost cache with it — the cost cache is correct only as a side effect of
the *effects* cache depending on research. Take `removed_by_tech` out of the pool, or
construct with `pEffectsProvider == nullptr` (a documented, supported configuration,
`ResearchManager.h:18-19`), and the cost silently freezes at its value from
`SetResearchTarget`. The class already owns `m_revision` and bumps it in
`AddDiscoveredTech`; the fix is to stamp and compare that alongside the provider version.

### [M] Social-rating expansion runs before pop and unit effects are collected
`src/game/faction/FactionEffectsPool.cpp:158-167` — `ExpandFactionLaneSocialRatingEffects`
accumulates `SocialRatingModifier` totals from whatever is in `factionEffects` *at that
point*: tile-yield rules, definition effects, buildings and policies. `CollectPopEffects_`
and `CollectUnitEffects_` append afterwards, so a faction-lane rating modifier declared by a
pop type or a unit component never reaches the faction-lane rating level. The same modifier
*does* count at base level, because `BaseManager::BuildBaseEffects_` runs
`ExpandSocialRatingEffects` over a list that already includes pop and unit contributions —
so the two lanes would disagree about the faction's Economy rating. No shipped config
declares a rating modifier on a pop type or unit component yet (only
`config/social_policies.json`, `config/factions/gaian/effects.json` and buildings do), so
this is latent rather than live. Move the expansion below every collection pass.

### [M] `removed_by_tech` is filtered after the effects it gates have already been expanded
`src/game/faction/FactionEffectsPool.cpp:169-178` — the `removedByTech` filter is the last
statement in `Rebuild_`, but grant expansion happened at line 62
(`ExpandGrantBuildingEffects`) and rating expansion at line 160. An effect that both carries
`removed_by_tech` and produces derived effects — a `GrantBuilding`, or a
`SocialRatingModifier` — has its expansion baked in before the gate is applied, so
discovering the tech removes the gate effect while everything it expanded into stays in the
pool. Only `config/tile_yield_rules.json` uses the field today (three
`TileResourceCap` entries, appended first and never expanded), so this is not currently
reachable; but `removedByTech` is parsed for every effect
(`src/game/effects/BonusEffectParser.cpp:329`) and documented as a general facility
(`include/game/effects/BonusEffect.h:399-401`). Filter before expanding.

### [M] The memoized pool is not bound to the faction it was built for
`src/game/faction/FactionEffectsPool.cpp:31-41` — `Get`/`GetVersion` take
`const Faction&` per call and the cache stores no identity, so validating with a different
faction returns the *other* faction's effects whenever the two happen to produce an equal
revision stamp — trivially true for two freshly created factions, where every contributor
revision is 0 and neither has bases. The header (`FactionEffectsPool.h:15-19`) presents this
as a deliberate design ("the pool holds no back-reference to its owner"), but the pool is a
by-value member of `Faction` and the only caller passes `*this`
(`src/game/Faction.cpp:628-636`), so the flexibility buys nothing and costs the invariant.
Take `const Faction&` in the constructor.

### [M] The rebuild stamp is re-collected after the rebuild instead of snapshotted before it
`src/game/faction/FactionEffectsPool.cpp:133,181` — `Validate_` collects the current
revisions into `m_scratchRevisions`, then `Rebuild_` walks every base a second time to
produce `m_cachedStamp`. Nothing in the collection path mutates a contributor today, so it is
currently equivalent, but stamping *after* reading means any future collector with a side
effect (a Lua hook, a lazily-materialised design) records a stamp newer than the content and
the cache stays permanently stale with no way to notice. `m_cachedStamp = m_scratchRevisions;`
is correct by construction and drops a full traversal.


### From: Faction — military, units, diplomacy, visibility

### [M] `IsUnitVisibleTo` re-collects tile area effects once per concealment channel
`src/game/faction/UnitVisibility.cpp:45` and `:64` — `CollectConcealmentChannels_` collects
the subject tile's area effects, then `HasDetectionCovering_` collects *the same tile's*
area effects again for every channel found. `TileEffectsContext::CollectAreaEffects` returns
a freshly allocated vector and scans neighbouring tiles and units each call. This function
is the per-frame UI draw and pick gate (`src/ui/world/WorldView.cpp:258`,
`src/ui/world/UnitMarkerRenderer.cpp:52`) and is called O(tiles × units) from
`UnitOrderExecutor::CollectVisibleHostileIds_`. Collect once in `IsUnitVisibleTo` and pass
the result down.

### [M] A `Detect` effect with no `ownerFaction` reveals concealed units to *every* faction
`src/game/faction/UnitVisibility.cpp:22` — `AppliesForFaction_` treats an absent
`ownerFaction` as "applies to all observers". Only territory-owned improvements get
`ownerFaction` populated (`src/game/effects/TileEffectsContext.cpp:124`), so today this is
correct by accident: `Sensor` is the only `Detect` source in config and it is
`owned_by_territory`. The first unit-component or base `Detect` — exactly the kind of thing
a modder adds — will strip cloak for all factions at once, silently. Either require an owner
on `Detect` effects or resolve the owner from the effect's source unit/base.


### From: Game core — turn pipeline, hooks, and validators

### [M] The effect validator's variant dispatch has no exhaustiveness guard
`ValidateEffectReferences` (`src/game/EffectReferenceValidator.cpp:52-74`) is an
`if/else if` chain of `std::get_if` over an 18-alternative `EffectVariant_t`. Adding a new effect
struct that carries a config id gets no compile-time reminder and simply goes unvalidated — the
exact "typo'd id becomes a silent no-op effect" failure this function exists to prevent. The
project already solves this elsewhere: `KindFor` in `include/game/effects/EffectEnums.h:92` is an
exhaustive switch backed by `-Werror=switch` in `src/CMakeLists.txt:138`, so adding a `StatId_t`
forces a decision. A `std::visit` with an overload set covering every alternative (with an
explicit no-op arm for the id-free ones) would give the validator the same property.

### [M] A missing registry silently disables validation instead of failing
`ValidateRequiredTechReferences` (`src/game/RequiredTechValidator.cpp:46-49`) returns early when
`rData.techRegistry` is null, turning the whole check into a no-op; the `GameDataContext` overload
of `ValidateEffectReferences` (`:131-134`) likewise passes raw `.get()` pointers whose null case
each check skips. `LoadGameData` always populates these, so the nullable path exists only for the
per-list test overload — but the guideline is to throw on an unexpected null, and here the
consequence of a wrong null is that every cross-config id check passes vacuously. Note
`tests/game/RequiredTechValidatorTests.cpp:85` currently encodes the silent skip as the intended
requirement, so this is a design decision to revisit rather than a plain bug: the
`GameDataContext` overloads should take the registries by reference and let the narrow, list-level
overload keep the nullable parameters the tests need. Related: both functions are hand-maintained
lists of `if (rData.X)` blocks over the same registries (`EffectReferenceValidator.cpp:142-205`,
`RequiredTechValidator.cpp:52-79`), so a new registry has to be remembered in three places
(`LoadGameData` plus both validators) and is silently unvalidated if it is not.


### From: Planetary Council — configuration and registries

### [H] Proposal `effects` are accepted in shapes the council can never apply
`src/game/council/CouncilProposalConfigParser.cpp:75` hands the effect array to
`BonusEffectParser::ParseEffects`, whose only source check (`ValidateScopeForSource`) rejects
`ThisPop`/`ThisUnit` and is deliberately permissive about everything else. But the council
consumes proposal effects in exactly two places: `CouncilEffects.cpp:30` keeps only
`Continuous` + `WorldGlobal`, and `CouncilOutcomeApplier.cpp:29` applies only `Instantaneous`
`GrantEnergy` (the `WorldParameter` branch is a documented TODO). So a proposal carrying a
`Continuous`/`FactionGlobal` bonus, an `Instantaneous` `StatModifier`, or a `ThisBase` effect
parses, validates, ships, passes a vote — and silently does nothing. A modder's only signal is
that the game does not change. Fix: give the council parser its own honored-shape check
(mirroring `ValidateScopeForSource`) that rejects any proposal effect outside the pair the
runtime implements, so unsupported combinations fail loudly at load.

### [M] `governor_effects` are parsed without checking the shapes the runtime keeps
`CouncilRulesConfigParser.cpp:44-51` parses the governor effect list with no scope/persistence
validation, but `CouncilEffects::SetGovernorEffects` (`CouncilEffects.cpp:44`) retains only
`Continuous` + `FactionGlobal` entries and `CouncilOutcomeApplier::ApplyGovernor` handles only
infiltration. A governor bonus written with the wrong scope is dropped without a word — the same
class of failure as the proposal effects above, on the config surface the architecture doc calls
out as "fully config-driven". Fix: validate the honored shapes in this parser.


### From: Planetary Council — runtime and outcomes

### [M] `ActiveEffect_t::config` pointers into a rebuilt vector outlive their guarantee
`include/game/council/CouncilEffects.h:21-25` claims the wrappers are kept with their backing
configs "so the non-owning `ActiveEffect_t::config` pointers stay valid across rebuilds", but
`RebuildWorld` clears and refills `m_worldConfigs` (`src/game/council/CouncilEffects.cpp:20-36`),
so any wrapper handed out earlier by `CollectWorldEffects()` — which returns a *copy* of the
vector (`PlanetaryCouncil.cpp:606`) — points into reassigned storage after the next rebuild. This
is latent rather than live today (I checked the consumers: `GameState::CollectWorldEffects` and the
stages use the result within the call, and no cache retains it), but it breaks the documented
contract that `config` "points into static config data"
(`include/game/effects/ActiveEffect.h:32`), and `ApplyPassedProposal_` rebuilds once per repeal
plus once on activation. Either state the real constraint ("valid until the next rebuild; never
retain") or give the configs stable storage (deque / `unique_ptr` nodes).


### From: Social engineering — policies and ratings

### [H] Do not accumulate ThisBase modifiers for FactionUnits expansion
`src/game/social-engineering/SocialRatingResolver.cpp:91-95` — `ExpandFactionLaneSocialRatingEffects` calls `AccumulateSocialRatings` on the entire faction pool. That pool still holds every base’s `ThisBase` `SocialRatingModifier`s (`FactionEffectsPool::CollectBuildingEffects_`). The resolver’s own contract (`SocialRatingResolver.h:17-22`) says accumulation is only meaningful on a context-filtered list; per-base expansion honors that after `FilterForBase`, but the faction-lane path does not. Axes whose level tables emit `FactionUnits` effects (morale, probe teams, etc.) will then treat N bases’ local modifiers as one faction total and apply the wrong unit bonuses. Production data currently keeps Those modifiers FactionGlobal-only, but fixtures already use `ThisBase` Growth and the architecture advertises base-local rating mods — so the bug is latent and mod-facing. Fix: accumulate only FactionWide-lane modifiers (or otherwise exclude `EffectLane_t::Base`) before expanding FactionUnits gameplay effects; add a regression that pairs `ThisBase` morale/probe with multiple bases.


### Additional hygiene / notes moved from other slices

- Effects known-gaps (uncached area scans, inert improvement faction-lanes pending territory) remain load-bearing future pressure — tracked in `docs/architecture/effects-system.md` Known Gaps; do not treat as missing features, but they constrain how territory/combat fill-in must plug in.

- `include/game/EffectReferenceValidator.h:29-31` — the doc comment lists the registries walked but omits factions, council proposals, council rules, and tile-yield rules, all of which the function does validate (`EffectReferenceValidator.cpp:187-205`). `include/game/RequiredTechValidator.h:9-10` has the same drift, omitting council proposals (`RequiredTechValidator.cpp:76-79`).
- `src/game/EffectReferenceValidator.cpp:34`, `src/game/RequiredTechValidator.cpp:28` — `ThrowBadReference_` / `ValidateRequiredTech_` use the trailing-underscore marker reserved for private methods on free functions in an anonymous namespace.
- `AppliesForFaction_` is duplicated verbatim in `src/game/faction/UnitVisibility.cpp:22` and
  `src/game/effects/TileEffectsContext.cpp:167`.
- `src/game/council/CouncilEffects.cpp:68-70` — silently skipping a null `config` contradicts
  "throw on unexpected null"; `AppendActiveEffects` always sets it, so this hides a corruption.
- `src/game/effects/BonusEffectParser.cpp:668-680` — `ValidateScopeForSource` only rejects `ThisPop` and `ThisUnit` on the wrong source kind, so a `ThisBase` effect on a unit component passes validation and is then silently dropped by every collector.
