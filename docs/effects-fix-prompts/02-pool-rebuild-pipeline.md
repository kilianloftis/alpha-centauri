# Package 2 — FactionEffectsPool rebuild / expand pipeline

**Date:** 2026-08-04  
**Source:** [`docs/effects-fix-packages.md`](../effects-fix-packages.md) Package 2; findings in [`docs/effects-model-review.md`](../effects-model-review.md)  
**Verdict:** **Confirm** the review’s canonical pipeline (collect → gate → expand → stamp) and every listed finding. **Amend** three details: (1) grant dedupe should seed from constructed buildings on `rBases` (not only scan effect `sourceId`s); (2) run `removed_by_tech` again after expansions so derived configs that carry the field also drop; (3) include the ResearchManager cost-cache fix in this package (not optional).

---

## Verified diagnosis

### 1. Social-rating expansion runs before pop/unit collection — **confirmed**

`Rebuild_` order today (`FactionEffectsPool.cpp:140-178`):

1. tile-yield rules, definition, buildings(+grants), social engineering  
2. **`ExpandFactionLaneSocialRatingEffects`** (`:158-161`)  
3. then `CollectPopEffects_` / `CollectUnitEffects_` (`:163-167`)  
4. then `removed_by_tech` erase (`:169-178`)

Faction-lane rating totals therefore omit any `SocialRatingModifier` declared on pop types or unit components (faction-lane scopes only — those collectors use `AppendFactionLaneEffects` / `IsFactionLane` at `:74` and `:89`). Base lane does **not** share this bug: `BaseManager::BuildBaseEffects_` expands ratings only after `CollectBaseLocalEffects_` has merged `CollectFromPops` (`BaseManager.cpp:276-295`). Same axis can disagree across lanes once mods land on pops/units. Latent in shipped data (policies / buildings / faction defs only).

### 2. `removed_by_tech` filtered after expansions — **confirmed**

Gate is last (`FactionEffectsPool.cpp:169-178`). Grant expand already ran inside `CollectBuildingEffects_` (`:62` → `ExpandGrantBuildingEffects`). Rating expand ran at `:160`. A `GrantBuilding` or `SocialRatingModifier` carrying `removedByTech` expands first; discovering the tech strips the gate effect but leaves derivatives. Field is general (`BonusEffect.h:399-401`, parsed at `BonusEffectParser.cpp:329`); live configs only put it on non-expanding `TileResourceCap`s (`config/tile_yield_rules.json` / fixtures) — latent for grants/ratings.

### 3. Faction-lane rating accumulates `ThisBase` modifiers — **confirmed [H]**

`ExpandFactionLaneSocialRatingEffects` (`SocialRatingResolver.cpp:91-95`) calls `AccumulateSocialRatings` on the **entire** faction pool. That pool still holds every base’s `ThisBase` building effects (`CollectBuildingEffects_` `:46-49` + `BaseManager::CollectBuildingEffects` origin tagging `:233-241`). Resolver contract (`SocialRatingResolver.h:17-22`) says accumulation is only meaningful on a context-filtered list; per-base `ExpandSocialRatingEffects` honors that after `FilterForBase`, faction-lane path does not.

Shipped `config/social_rating_effects.json` emits `FactionUnits` gameplay effects for **morale** and **probe** axes (`:97-150`, `:250-283`). N bases with `ThisBase` morale/probe modifiers would inflate the faction unit bonus. Production data keeps those modifiers FactionGlobal-only today; fixtures already use `ThisBase` Growth (`RatingTests.cpp:115-128`, `growth_shrine`).

### 4. Grant expansion double-counts constructed buildings — **confirmed [H]**

`ExpandGrantBuildingEffects` (`ActiveEffect.cpp:141`) seeds `processedGrantedIds` empty; only records grant expansions (`:174-176`, `:190-205`). Grant-vs-grant dedupe is tested (`GrantExpansionTests.cpp:95-108`); grant-vs-constructed is not. Input already contains constructed buildings’ continuous effects (`BuildingManager::CollectEffects` `:54-63` uses `pBuilding->id` as `sourceId`). `GrantChainContains_` (`ActiveEffect.cpp:111-129`) only walks the grant’s own `sourceId` chain — does not see a sibling constructed building. Canonical SMAC case (Command Nexus granting Perimeter Defense when the base already built it) is latent: no shipped `GrantBuilding` yet (`config/buildings/README.md:158`).

### 5. Memoized pool not bound to owning faction — **confirmed**

`Get` / `GetVersion` take `const Faction&` per call (`FactionEffectsPool.cpp:31-41`); cache stores no identity. Equal revision stamps across two factions (trivially two empty new factions — all contributor revisions 0) return the other faction’s cached pool. Header presents this as deliberate (`FactionEffectsPool.h:15-19`), but the pool is a by-value member of `Faction` (`Faction.h:241`) and the only caller passes `*this` (`Faction.cpp:628-636`).

### 6. Rebuild stamp re-collected after rebuild — **confirmed**

`Validate_` collects into `m_scratchRevisions` (`:133`), then `Rebuild_` ends with a second `CollectRevisions_` into `m_cachedStamp` (`:181`). Equivalent today (collectors are side-effect free) but any future collector mutation records a stamp newer than content → permanent stale cache. `m_cachedStamp = m_scratchRevisions` after rebuild is correct by construction.

### 7. Research cost cache keyed only on effects version — **confirmed [H]** (include in package)

`ComputePointsNeeded_` feeds `m_discoveredTechs.size()` into cost inputs (`ResearchManager.cpp:116-118`) but `RevalidatePointsNeeded_` only compares `GetEffectsVersion()` (`:131-140`) and **returns early when `pEffectsProvider == nullptr`** (`:133-135`) — a documented supported mode (`ResearchManager.h:18-19`). Correctness today is a side effect of `CollectRevisions_` stamping research revision (`FactionEffectsPool.cpp:121`), so tech discovery bumps the pool and drags the cost cache. Mid-research tech steal / treaty paths call `AddDiscoveredTech` (bumps `m_revision` at `:191`) without touching the research target. Fix: stamp and compare `ResearchManager`’s own revision alongside the provider version; revalidate on research revision even with a null provider.

### 8. Rating lane split introduced by package 1 — **new; package 2 must resolve**

Added after package 1 landed. `Faction::GetActiveEffects()` is now the *composed* pool (local
`FactionEffectsPool` + peer `WorldGlobal` + council extras), while `Rebuild_` — and therefore
`ExpandFactionLaneSocialRatingEffects` — still runs over the **local** pool only.

Consequence: a `SocialRatingModifier` arriving as a council effect or a peer faction's
`WorldGlobal` is counted by `BaseManager::GetEffectiveSocialRating`
(`BaseManager.cpp:316-326`, which accumulates over the composed pool) but never reaches
faction-lane expansion. Same axis, two answers — the shape package 1 was meant to eliminate,
displaced one level up. Latent only: no shipped config declares a rating modifier at those
scopes, and `BonusEffectParser::ValidateScopeForSource` (`BonusEffectParser.cpp:668-681`)
gates only `ThisPop` / `ThisUnit`, so a mod can reach it.

Two acceptable resolutions — pick one deliberately and document it next to
`AccumulateSocialRatings`:

- **Compose then expand**: give faction-lane expansion the same composed input the base lane
  sees. Costs a rebuild trigger on every world-stamp move, so it interacts with the memo
  keying in finding 5.
- **Local-only ratings**: restrict rating accumulation on *both* lanes to the local pool, and
  state that ratings are a faction-internal axis that world/council effects cannot move.
  Cheaper; needs `ValidateScopeForSource` to reject `WorldGlobal` `SocialRatingModifier` so
  the restriction fails loud at load rather than silently (coordinate with package 5).

An in-code marker at `BaseManager.cpp:316` points here.

### BaseManager rating expand — **no change required for order**

`BuildBaseEffects_` already expands **after** filter + pop merge (`BaseManager.cpp:286-295`). Package 2 only needs it to keep consuming a correctly ordered faction pool; do not move base-lane expansion into the faction pool.

### Package interactions

| Package | Interaction |
|---------|-------------|
| **1** (single pool) | May already have changed `IEffectsProvider` / who contributes to the pool. Land after or rebase onto package 1; **do not** re-solve world/council folding here. Revision stamp must still see every contributor package 1 adds. **See finding 8 below — package 1 as implemented hands package 2 a new lane split to resolve.** |
| **3** (ActiveEffect contracts) | May change origin tagging / `CollectBuildingEffects`. Grant pre-seed from `GetBuildings()` is robust to origin-tag refactors; coordinate if both touch `ExpandGrantBuildingEffects`. |
| **5/7** | Independent. |

---

## Design decision

### Chosen

Canonical `Rebuild_` pipeline:

1. **Collect** all raw continuous contributors (tile-yield rules, faction definition, **raw** constructed buildings — no grant expand yet — social engineering, pop faction-lane, unit faction-lane).
2. **Gate** `removed_by_tech` (erase effects whose tech is discovered).
3. **Expand grants** via `ExpandGrantBuildingEffects`, with `processedGrantedIds` **pre-seeded from constructed buildings** on each base in `rBases`: for every `BuildingManager::GetBuildings()` entry, insert `{pBase, id}` and `{nullptr, id}` (any construction already contributes that building’s FactionGlobal effects once).
4. **Expand faction-lane social ratings** with accumulation restricted to modifiers whose `LaneFor(scope) == EffectLane_t::FactionWide` (exclude `EffectLane_t::Base` / `ThisBase`). Still emit only `FactionUnits` gameplay effects from the level table (existing filter at `SocialRatingResolver.cpp:121-124`).
5. **Gate `removed_by_tech` again** on the full list (derivatives from grants/rating tables may carry the field).
6. **Stamp** `m_cachedStamp = m_scratchRevisions` (snapshot from `Validate_` before rebuild); do not re-walk bases.
7. **Bind** `const Faction&` in `FactionEffectsPool`’s constructor; `Get` / `GetVersion` use the bound faction (drop per-call `Faction&` unless a test seam truly needs it — prefer delete).
8. **ResearchManager:** record `m_costResearchRevision` alongside `m_costEffectsVersion`; `RevalidatePointsNeeded_` recomputes when either differs. With null provider, still revalidate when research revision changes (tech count is a direct cost input).

### Rejected

| Alternative | Why not |
|-------------|---------|
| Only move rating expand below pops/units; leave grant expand inside `CollectBuildingEffects_` | Grant expand must also sit **after** the tech gate; keep one ordered pipeline in `Rebuild_`. |
| Pre-seed grants only by scanning effect `sourceId`s | Works today (`BuildingManager` uses building id) but couples dedupe to sourceId string shape and misses a constructed building that contributed zero continuous effects. `GetBuildings()` matches the SMAC rule directly. |
| Accumulate faction-lane ratings with `IsFactionLane` (FactionWide **or** FactionUnits) | `FactionUnits`-scoped rating *modifiers* are not a supported accumulation context; review asks for FactionWide-only. Base-lane modifiers must stay out. |
| Single end-of-Rebuild `removed_by_tech` filter (no pre-expand gate) | Gated `GrantBuilding` / `SocialRatingModifier` would still expand. |
| Leave ResearchManager as optional follow-up | Small, independent, and the silent freeze with null provider is a real [H]; package already lists it. |
| Re-expand FactionWide gameplay effects in the faction pool | Architecture / resolver comments: base-lane economy etc. stay on `ExpandSocialRatingEffects` after `FilterForBase` only. |

---

## Implementation plan

1. **`FactionEffectsPool`**
   - Constructor: take `const Faction& rFaction` (store `m_rFaction`); keep registry / base-list revision / tile-yield / social-rating deps.
   - `Faction` initializer: pass `*this` (reference only stored; no dereference during ctor).
   - Split building collection: raw concatenate of `BaseManager::CollectBuildingEffects` vs grant expand in `Rebuild_`.
   - Reorder `Rebuild_` to the pipeline above; helper `ApplyRemovedByTech_(FactionEffects_t&, const ResearchManager&)` to avoid duplicating the erase predicate.
   - `Validate_`: `CollectRevisions_` → compare → `Rebuild_`; `Rebuild_` assigns `m_cachedStamp = m_scratchRevisions`.
   - Update class comment: pool is bound to its owning faction.
2. **`ExpandGrantBuildingEffects`**
   - Before the scan loop, for each non-null base in `rBases`, for each building in `GetBuildingManager().GetBuildings()`, insert `{pBase, id}` and `{nullptr, id}` into `processedGrantedIds`.
   - Keep grant-vs-grant and cycle-guard behavior unchanged.
3. **`ExpandFactionLaneSocialRatingEffects`**
   - Build the accumulation list from effects where `rEffect.config && LaneFor(rEffect.config->scope) == EffectLane_t::FactionWide` (or filter in place before `AccumulateSocialRatings`). Update header contract comments.
4. **`ResearchManager`**
   - `mutable uint64_t m_costResearchRevision`; set in `ComputePointsNeeded_`; `RevalidatePointsNeeded_` triggers on research revision and/or effects version; null-provider path still watches research revision when a target is set.
5. **Docs:** `docs/architecture/effects-system.md` — document collect → gate → grant expand → faction-lane rating expand → gate → stamp; note grant-vs-constructed waste; note FactionWide-only accumulation for faction-lane ratings; fix stale “both expand functions take `BaseEffects_t`” wording (`:325-327` vs `ExpandFactionLaneSocialRatingEffects` taking `FactionEffects_t`).
6. **Call sites / tests** — update any direct `FactionEffectsPool` construction (only `Faction` today); extend grant + rating + research cache tests.

---

## Test plan

Requirement-based (assert the rule, not today’s accident):

1. **Grant vs constructed (new):** Base has `granted_hall` constructed **and** `grantor_local` (or equivalent). After expand, `granted_hall`’s ThisBase minerals appear **once** (not 2×). Extend `tests/effects/GrantExpansionTests.cpp`.
2. **Grant vs constructed, global grant:** Base A constructed `granted_hall`; base B has not; faction-global grantor present. A does not get a second ThisBase copy; B still receives the grant clone; FactionGlobal energy from the hall appears once.
3. **Grant-vs-grant regression:** existing “granted twice expands once” (`GrantExpansionTests.cpp:95`) stays green.
4. **Faction-lane rating ignores ThisBase (new):** Two bases each with a `ThisBase` morale (or probe) `SocialRatingModifier` building; faction-wide policy contributes a known FactionGlobal amount. `ExpandFactionLaneSocialRatingEffects` / unit-facing pool must match **policy-only** level, not policy + 2× shrine. Pair with multi-base fixture (Growth shrine pattern in `RatingTests.cpp`).
5. **Rating expand sees pop/unit faction-lane mods (new):** Faction-lane `SocialRatingModifier` on a pop type or unit component changes the faction-lane expanded level; collecting that source after rating expand must not be possible (order pin).
6. **`removed_by_tech` before expand (new):** Effect list containing a `GrantBuilding` (or rating mod) with `removed_by_tech` for a discovered tech must **not** contribute granted/rating derivatives after rebuild.
7. **Pool identity (new):** Two factions, empty/equal revision stamps; each `GetActiveEffects()` returns that faction’s definition effects, not the other’s (bind + call `Get` on each).
8. **Research cost + tech count (new):** With a null effects provider (or a fake provider whose version is held constant), `AddDiscoveredTech` while a target is set must change `GetPointsNeededForCurrentTech` when cost inputs depend on tech count. Existing mid-research TechCost effect test (`EffectsCacheTests.cpp:106`) remains.
9. **Tile cap lift regression:** `BaseIntegrationTests` / tile restriction — discovering `gene_splicing` still lifts caps (`BaseIntegrationTests.cpp:180-181`).

Build/test via `./bd` only; prefer filters `[effects][grant]`, `[effects][rating]`, `[research][cache]`.

---

## AI implementation prompt

```
You are implementing Package 2 of the effects-model remediation for the Alpha Centauri C++ rebuild at /home/martok/alpha-centauri.

## Goal
Make FactionEffectsPool rebuild a single ordered pipeline: collect raw continuous effects → apply removed_by_tech → expand grants (with grant-vs-constructed dedupe) → expand faction-lane social ratings (FactionWide modifiers only) → apply removed_by_tech again → stamp cache from the pre-rebuild revision snapshot. Bind the pool to its owning Faction. Fix ResearchManager’s research-cost cache so it keys on research revision as well as effects version.

## Read first
- docs/effects-fix-prompts/02-pool-rebuild-pipeline.md (this package’s analysis — follow its design)
- docs/effects-fix-packages.md Package 2
- .cursor/rules/coding-guidelines.md
- docs/architecture/effects-system.md (CollectActiveEffects / Social Ratings — update when you change the pipeline)
- Key code: FactionEffectsPool.cpp/.h, ActiveEffect.cpp (ExpandGrantBuildingEffects), SocialRatingResolver.cpp/.h, BaseManager.cpp (BuildBaseEffects_ — do not break its post-filter rating expand), ResearchManager.cpp/.h, Faction.cpp/.h
- Tests: tests/effects/GrantExpansionTests.cpp, RatingTests.cpp, EffectsCacheTests.cpp

## Constraints
- Follow coding guidelines: references over pointers, throw over silent defaults, no back-compat shims, SOLID, naming/braces.
- Build and test only via ./bd (never raw cmake/make/ctest).
- Do not hardcode game balance numbers in C++.
- Out of scope: Package 1 world/council folding into the provider; Package 3 origin-tag rewrite; parser strictness; tile preview API.
- If Package 1 already landed and touched FactionEffectsPool / IEffectsProvider, rebase onto it — preserve any new contributors in CollectRevisions_ / Rebuild_ collect phase.
- Keep FactionEffects_t / BaseEffects_t lane typing; do not re-expand FactionWide rating gameplay effects in the faction pool (base path only).

## Required behavior changes

### A. FactionEffectsPool::Rebuild_ pipeline
1. Collect raw: tile-yield rules, definition effects, raw building effects from every base (NO grant expand yet), social engineering, pop faction-lane, unit faction-lane.
2. Erase effects with removedByTech discovered (same predicate as today).
3. ExpandGrantBuildingEffects(rawBuildingsOrCombined, registry, bases).
4. ExpandFactionLaneSocialRatingEffects (see B).
5. Erase removed_by_tech again (derivatives).
6. m_cachedPool = move; m_cachedStamp = m_scratchRevisions (from Validate_); ++m_version. Do NOT call CollectRevisions_ again at end of Rebuild_.

### B. Faction-lane rating accumulation
In ExpandFactionLaneSocialRatingEffects, accumulate SocialRatingModifier only from effects whose LaneFor(scope) == EffectLane_t::FactionWide. Continue to append only FactionUnits gameplay effects from the level table. Update SocialRatingResolver.h comments to state this contract.

### C. Grant vs constructed
In ExpandGrantBuildingEffects, before the expansion loop, pre-seed processedGrantedIds from constructed buildings:
  for each pBase in rBases (skip null):
    for each building in pBase->GetBuildingManager().GetBuildings():
      insert({pBase, building->id});
      insert({nullptr, building->id});
Keep existing grant-vs-grant dedupe and GrantChainContains_ cycle guard. Unknown grant id still throws.

### D. Bind pool to Faction
FactionEffectsPool constructor takes const Faction& (member reference). Get/GetVersion no longer take a Faction parameter — use the bound faction. Update Faction to pass *this. Update header comments (delete “no back-reference” rationale).

### E. ResearchManager cost cache
Stamp m_costResearchRevision = GetRevision() in ComputePointsNeeded_. RevalidatePointsNeeded_ recomputes when research revision differs OR (when provider non-null) effects version differs. With null provider and an active target, tech discovery must still recompute cost. Keep existing TechCost-via-provider behavior.

### F. Architecture doc
Update docs/architecture/effects-system.md for the new pipeline, grant-vs-constructed waste, FactionWide-only faction-lane rating accumulation, and correct types for ExpandFactionLaneSocialRatingEffects.

## Acceptance criteria
- New/updated tests cover: grant+already-constructed (local and global grant); ThisBase rating mods do not inflate FactionUnits expansion across multiple bases; rating expand observes pop/unit faction-lane modifiers; removed_by_tech prevents grant/rating derivatives; two factions do not share a pool when stamps match; research cost updates on AddDiscoveredTech with null or frozen provider version.
- Existing GrantExpansionTests grant-vs-grant / cycle cases and EffectsCacheTests TechCost mid-research case stay green.
- ./bd build and ./bd test with appropriate filters pass for touched areas; mention any failures and whether they are requirement changes vs bugs.
- No drive-by refactors outside this package’s files.

## Do not
- Fold world/council effects into the pool (Package 1).
- Change ResolveStat rounding or base-level % seeding (Package 1).
- Rewrite origin tagging in BaseManager::CollectBuildingEffects (Package 3).
- Weaken or delete assertions to force green tests — fix code or update tests only when the requirement changed (document that).
```
