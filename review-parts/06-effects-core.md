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
