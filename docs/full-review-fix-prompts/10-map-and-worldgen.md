# Package 10 — Map runtime and world generation

Findings re-verified against the tree at `d7858ed` before implementation. The review predates
packages 1–9, so every `path:line` below was re-read rather than trusted.

## Verified diagnoses

### [H] Gameplay improvement queries use sprite content ids

`src/game/map/TileLayerResolver.cpp:55-72` probes `rTile.HasImprovement(TileLayerContent::k_Farm)`
where `k_Farm == "farm"`, but `config/improvements.json:147` declares the id as `"Farm"`. Verified:
the Vegetation and Road layers can never populate, and `ResolveImprovementLayer_:86-88` skips on
the same lowercase strings, so Farm/Forest/Road fall through into the Improvement layer instead.

Two id domains are genuinely in play and both are needed — the config id is what the tile holds,
the lowercase content id is what a sprite atlas keys on. The bug is that one constant was used for
both. There is no single place naming the config ids: `TerraformSpread.cpp:133` writes
`"KelpFarm"`/`"Forest"` as `const char*`, `MoveCostCalculator.cpp:25` has its own `k_RoadId`,
`Engine.cpp:426` and `TileRenderer.cpp:102` write `"Forest"` inline.

**Chosen:** add `include/game/map/ImprovementIds.h` holding the well-known config ids that C++
references by name, and point the resolver's *probes* at those while its *returns* stay
`TileLayerContent`. The two domains then differ visibly at every call site.

**Rejected:** renaming the config ids to lowercase. It is a save/mod-facing wire format and the
collision would remain — the resolver would still be conflating a render key with a rules key.

### [H] Rivers are computed before landmarks reshape the terrain

`WorldGenerator.cpp:49-55` runs `GenerateAquifers_` (which ends in `RecomputeRivers`, `:278`)
before `GenerateLandmarks_`. Verified: `ApplyMountPlanetSculpt_` (`LandmarkGeneration.cpp:97-128`)
raises elevation and rewrites rockiness afterwards, and `BoreholeCluster` stamps `ThermalBorehole`,
whose `terminates_river: true` is read by `TileTerminatesRiver` (`RiverGeneration.cpp:44-61`).
Rivers therefore flow down pre-sculpt slopes and straight through boreholes.

**Chosen:** the review's second direction — move landmark placement ahead of the aquifer/river
stage, giving `elevation → moisture → rockiness → fungus → landmarks → aquifers+rivers → bonuses`.
Checked the dependencies this reorder could break:

- `PlaceFungus` reads neither rivers nor moisture, so fungus may stay ahead of landmarks (and must,
  because `TheRuins` sets fungus on its own footprint).
- No landmark in `config/improvements.json:537-640` excludes `River` or `Fungus`; they exclude
  `@landmark` and `@resource_bonus` only. So placing landmarks before rivers exist does not change
  which anchors are eligible.
- Bonus placement already runs last, which is what `@resource_bonus` needs.

**Rejected:** a second `RecomputeRivers` call after landmarks. It fixes the same two symptoms but
leaves the pipeline with a stage that has to be re-run to be correct, and it does nothing for the
stale orographic moisture on sculpted peaks.

**Deferred, with reason:** orographic moisture is still computed from pre-sculpt elevation. Moving
`GenerateMoisture_` after landmarks would re-roll every tile's moisture *after* landmark anchors
were chosen against the old values, and moisture tiers are themselves `HasFeature` ids that
`CanBuildImprovement` consults — a landmark could end up on a tile that now excludes it. Recording
as a TODO in the pipeline rather than guessing at the resolution.

### [H] `excludes` is enforced in one direction only

`ImprovementConfigParser.cpp:96-104` checks `rCandidate.excludes` against the tile and nothing
else. Verified: `MountPlanet` excludes `@resource_bonus`, but `Nutrients`/`Minerals`/`Energy`
declare no `excludes`, so the shared helper permits stacking a bonus onto a landmark.
`TileBonusGeneration.cpp:18-50` compensates with a private reverse scan `TileExcludesBonus_`;
`LandmarkGeneration.cpp:162` and the terraform callers do not, so they have the hole.

**Chosen:** fold the reverse scan into `CanBuildImprovement` and delete `TileExcludesBonus_`. One
predicate, every caller.

**Rejected:** requiring symmetric `excludes` in data. It doubles every declaration and makes an
asymmetric pair a silent data bug rather than an impossible state.

### [M] Fungus is mutated while probing spread eligibility

`TerraformSpread.cpp:89-100` clears `SetHasFungus(false)`, calls `CanBuildImprovement`, then
restores it — inside a selection pass over neighbours. Verified: `SetHasFungus` calls
`RefreshTerrainFeatures_`, so this rebuilds the tile's cached feature vector twice per candidate,
and any re-entrant reader observes a tile that lies about its fungus.

**Chosen:** give `CanBuildImprovement` a `clearedFeatureId` parameter naming a feature the caller
will remove as part of the same placement. The spread pass passes `Fungus` and touches nothing.
Applies to both directions of the exclude check.

### [M] Missing Forest/KelpFarm config is indistinguishable from "no eligible neighbour"

`TerraformSpread.cpp:139-143` returns `false` when `Find(improvementId)` is null. Verified: a
mod that omits `Forest` silently disables forest growth for the whole game, and
`ValidateTerrainFeatures` does not cover these two ids (they are improvements, not
`TerrainFeature_t`). Fixed by using `Get` (which throws).

### [M] `TerritoryMap::Rebuild` returns silently when unsized

`TerritoryMap.cpp:116-121` verified. Additionally `ClaimFromBase_:56-63` sizes `visited` from the
*grid's* width/height while indexing it with *world* coordinates, and never checks that the two
agree — a desynced `Reset` writes out of bounds at `visited[index(ox, oy)]` before any bounds test.

### [M] `WorldMap` hands out its owning tile vector

`WorldMap.h:34-35` verified. `std::vector<std::unique_ptr<Tile>>&` lets any of the 53 callers
`clear()` the map or reseat an element while `UnitPositionIndex`, `WorkedTileIndex`, bases and
units hold raw `Tile*`.

**Chosen:** return `std::span<const std::unique_ptr<Tile>>`. Ownership operations
(`clear`, `reset`, assignment) stop compiling; `pTile->SetElevation(...)` still works, because
`unique_ptr::operator->` on a const `unique_ptr` yields a non-const `Tile*`. One call site
(`TileEffectsContext.cpp:266`) spells the element type explicitly and needs updating; the rest use
`auto&` and are unaffected.

**Rejected:** switching storage to `std::vector<Tile>` and exposing `std::span<Tile>` /
`std::span<const Tile>`. It is the stronger design — it also fixes the const overload handing out
mutable tiles — but it churns all 53 call sites across 30 test files for a const-correctness
problem the finding does not raise. Recorded here as the follow-up if that leak is ever addressed.

### [M] Non-positive dimensions produce an empty world instead of throwing

`WorldMap.cpp:8-21` and `WorldGenerator.cpp:47` verified: neither validates, `GenerateElevation_`
returns early on `tileCount <= 0`, and every later stage no-ops. Throwing in the `WorldMap`
constructor covers both findings at once, since `Generate` constructs one.

### [M] Mount Planet sculpt knobs are hardcoded

`LandmarkGeneration.cpp:114-121` verified: `3500`, `1000 + t * 2500`, and `1.5f` are literals, and
`"mount_planet"` appears at `:175`, `:204` and in `LandmarkConfigParser.cpp:88`.

**Chosen:** a `sculpt` object on the sculptor shape (`peak_elevation`, `base_elevation`,
`rocky_core_radius`) with the current values as defaults, so `config/worldGen/landmarks.json` can
tune a volcano without a rebuild. The *algorithm* stays in C++ and stays selected by `sculptorId`.

**Rejected:** a general sculptor registry. There is one sculptor; per the coding guidelines an
extension point with no second implementation is speculative.

### [M] Empty landmark footprints are skipped silently

`LandmarkGeneration.cpp:236-240` verified. `ExpandMask_` returns empty for a mask with no `X`/`x`
cells, and `LandmarkConfigParser.cpp:76-83` only rejects a missing or empty `rows` array — a mask
of all-blank rows parses and then the landmark is never attempted. Rejecting at parse time is the
better locus: it names the file and the id.

### [M] Resolved seed is not recorded

Partly closed in Package 4 — `Engine.cpp:154` resolves one session seed and prints it, and
`WorldGenerator::Generate` takes it as a parameter. Two gaps remain:

1. `GenerateElevation_:72` re-reads `rConfig.seed` and seeds `FbmNoise` from it, bypassing the
   resolved seed entirely. The map is not a function of the value the composition root reports.
   Fixed by deriving the noise seed from `m_rng`.
2. Persisting the seed so a finished game can be replayed. **Deferred:** there is no save system
   (no serialization module anywhere in `src/`). Writing it back through
   `GameSettings::SetMapGeneration` would be wrong — it would turn `seed: 0` ("pick one") into a
   fixed seed for every subsequent new game. The `Engine.cpp` TODO is repointed from this package
   to the save system.

## Review follow-ups

The review found one regression I introduced, one fix of mine that did nothing, and one hole in
the fix that made it inconsistent between world gen and play.

1. **[Regression] `Tile::SetElevation`'s new bound made an existing bug fatal.**
   `TerraformRules.cpp` gates sea-former `LowerLand` on nothing at all (`return true`), then
   subtracts a flat 1000. On a deep-ocean tile that used to store an out-of-range elevation
   silently; with the bound it throws out of order execution, and nothing catches between there
   and `main()`. `RaiseLand` was already clamped, so `LowerLand` was the only unclamped path.
   Both the gate and the mutation now stop at `k_MinElevation`, with a TODO for the real SMAC
   rule. `TerraformTests` covers it.

2. **[Vacuous fix] The `Find` → `Get` change in `TerraformSpread` is unreachable.**
   `TrySpreadTerraformFromTile` returns early on `!rOrigin.HasImprovement(improvementId)`, and a
   tile cannot hold an improvement the registry never defined — so the missing-config case still
   read as "nothing spread". Moved to load time instead: `ValidateTerrainFeatures` now requires
   `Forest` and `KelpFarm`, which is the review's own alternative direction. The `Get` call stays,
   now genuinely justified by the load-time proof.

3. **[Inconsistency] The new reverse `excludes` check saw nothing during world generation.**
   The forward leg uses `Tile::HasFeature`, which resolves terrain intrinsically; the reverse leg
   reads `GetTerrainFeatures()`, which is empty until `BindImprovements`. World gen never bound
   its tiles, so a terrain feature's `excludes` were enforced in play but not during generation —
   a generated map could contain a placement the rules forbid. `WorldGenerator::Generate` now
   binds the grid before its first stage. (The pre-existing `TileExcludesBonus_` had the same
   hole, so this was latent before the package too.)

4. **[Hollow test] The terrain-feature branch had no coverage.** No terrain feature in the test
   fixture declared `excludes`, so deleting the entire `GetTerrainFeatures()` term left all tests
   green. Fixture `River` now excludes `Mine` (which excludes nothing), and the test asserts the
   asymmetric pair. Verified by reverting.

5. **Elevation bounds are validated at config load**, in the preset and landmark-sculpt parsers,
   so a modder who widens the range gets a message naming the file and id rather than an
   `out_of_range` from deep inside generation.

Also applied: `ImprovementIds` now covers the remaining scattered literals (including
`MoveCostCalculator`'s duplicate `k_RoadId`); `WorldMap::GetTiles`'s redundant non-const overload
and `GetTile`'s now-unreachable width guard removed; comments that narrated the change rewritten
to describe the code.

Not applied: `TerraformInputController`'s 22-entry keybinding table still spells its improvement
ids inline. Converting four of twenty-two to constants would be worse than leaving it — the table
wants to be config, which is Package 14's theme.

## Deferred from this package

- Orographic moisture on sculpted peaks (above) — TODO in the pipeline.
- `std::vector<Tile>` storage for `WorldMap` (above).
- Save-game persistence of the session seed (above).
- The `FungusGeneration` frontier dedupe changes patch-growth output as well as its cost (the
  review scoped it as performance-only). Both eligibility predicates are monotone — a tile
  dropped at pop time could never have succeeded later — so it is not a correctness change, but
  same-seed fungus layouts differ from before and nothing pins them.
