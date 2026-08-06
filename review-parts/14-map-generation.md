## Map — world generation and config

**Files:** `src/game/map/FbmNoise.cpp`, `include/game/map/FbmNoise.h`,
`src/game/map/FungusGeneration.cpp`, `include/game/map/FungusGeneration.h`,
`src/game/map/ImprovementConfigParser.cpp`, `include/game/map/ImprovementConfigParser.h`,
`src/game/map/LandmarkConfigParser.cpp`, `include/game/map/LandmarkConfigParser.h`,
`src/game/map/LandmarkGeneration.cpp`, `include/game/map/LandmarkGeneration.h`,
`src/game/map/MapGenerationConfig.cpp`, `include/game/map/MapGenerationConfig.h`,
`src/game/map/RiverGeneration.cpp`, `include/game/map/RiverGeneration.h`,
`src/game/map/TileBonusGeneration.cpp`, `include/game/map/TileBonusGeneration.h`,
`src/game/map/WorldGenDecorationConfigParser.cpp`, `include/game/map/WorldGenDecorationConfigParser.h`,
`src/game/map/WorldGenPresetConfigParser.cpp`, `include/game/map/WorldGenPresetConfigParser.h`,
`src/game/map/WorldGenerator.cpp`, `include/game/map/WorldGenerator.h`,
`include/game/map/MapUtils.h`, `include/game/map/WorldGenPresetRegistry.h`,
`include/game/map/WorldGenDecorationConfig.h`, `include/game/map/MoistureGeneration.h`,
`include/game/map/RockinessGeneration.h`, `include/game/map/TileLayer.h`,
`include/game/map/ImprovementRegistry.h`, `include/game/map/LandmarkConfig.h`,
`include/game/map/WorldGenPresetConfig.h`

**Assessment:** World gen is well factored for an in-progress system: elevation/FBM,
moisture/rockiness helpers, fungus/landmark/bonus placers, and JSON decoration/preset parsers
are separate and mostly throw on bad config. Cylinder wrap in `MapUtils` / noise sampling is
clear and consistent. The dominant weakness is pipeline ordering and exclusivity semantics —
landmarks reshape terrain and stamp `terminates_river` features after rivers are finalized,
and improvement coexistence is only half-enforced in the shared helper.

### [H] Reflow rivers after landmarks (elevation sculpt and terminators)
`src/game/map/WorldGenerator.cpp:51-57` — `GenerateAquifers_` calls `RecomputeRivers`
(`:280`) before `GenerateLandmarks_`. `ApplyMountPlanetSculpt_` then raises elevations and
changes rockiness (`LandmarkGeneration.cpp:97-128`), and `BoreholeCluster` stamps
`ThermalBorehole` (`terminates_river: true`) with no second reflow. Rivers keep flowing on
pre-landmark slopes and through boreholes that should stop them; orographic moisture is also
left stale on sculpted peaks. Direction: call `RecomputeRivers` (and reconsider moisture) after
landmark placement, or move sculpt/terminator landmarks before the aquifer/river stage.

### [H] Make `CanBuildImprovement` enforce both directions of `excludes`
`src/game/map/ImprovementConfigParser.cpp:96-104` — only `rCandidate.excludes` are checked
against the tile. Landmark configs exclude `@resource_bonus`, but `Nutrients`/`Minerals`/
`Energy` declare no excludes, so the shared helper would allow stacking bonuses on landmarks.
`TileBonusGeneration.cpp:18-63` duplicates a reverse scan (`TileExcludesBonus_`) to paper over
that; `LandmarkGeneration` and terraform callers do not. Direction: fold the reverse check into
`CanBuildImprovement` (or require symmetric excludes in data and delete the local helper) so
every placement path shares one rule.

### [M] Surface Mount Planet sculpt knobs in landmark/sculptor config
`src/game/map/LandmarkGeneration.cpp:97-128` — peak elevation caps (`3500`, `1000+t*2500`),
rocky core radius (`1.5f`), and the `"mount_planet"` id are hardcoded C++. Adding or tuning a
volcano landmark requires a code change despite `landmarks.json` already carrying radius.
Direction: put sculpt parameters on the shape/config (or a small sculptor table) and keep only
the algorithm in C++.

### [M] Reject invalid map dimensions instead of returning an empty world
`src/game/map/WorldGenerator.cpp:49-72` — `Generate` always constructs `WorldMap(width,
height)` with no check; `GenerateElevation_` returns early when `tileCount <= 0`, and later
stages no-op on an empty grid. Negative or zero sizes therefore produce a silent empty map
rather than a throw (against project preference). Direction: validate `width`/`height` (and
optionally seed) at the start of `Generate` and throw.

### [M] Do not silently skip empty landmark footprints
`src/game/map/LandmarkGeneration.cpp:236-240` — if `ExpandLandmarkShape` returns empty
(e.g. a mask with no `X`/`x` cells from `ExpandMask_` at `:66-94`), placement continues with
no error and that landmark is never attempted. Parser accepts such masks
(`LandmarkConfigParser.cpp:74-83`). Direction: throw at parse or place time when the expanded
footprint is empty.

### [M] Record the resolved seed when `seed == 0`
`src/game/map/WorldGenerator.cpp:44-47` — a zero seed picks `steady_clock` entropy into a
local and seeds `m_rng`, but never writes the effective seed back to
`MapGenerationConfig_t`. A “random” new game cannot be regenerated from settings. Direction:
return the resolved seed from `Generate` or write it through an out-parameter / mutable config
field the settings layer can persist.

### [L] Convention and hygiene items
- `src/game/map/WorldGenerator.cpp:302-306` — `RandomInt_` is unused dead code; remove or use it.
- `src/game/map/ImprovementConfigParser.cpp:100-101` — `CanBuildImprovement` uses a braceless
  `return false;` (brace style).
- `src/game/map/LandmarkConfigParser.cpp:17-22`, `MapGenerationConfig.cpp:15-18`,
  `WorldGenDecorationConfigParser.cpp:18-22` — identical `ToLower_` helpers copied three times.
- `src/game/map/WorldGenPresetConfigParser.cpp:58-65` — `ParseType_` is case-sensitive via
  `magic_enum::enum_cast`, while `ParseErosiveForces` lowercases; inconsistent wire tolerance.
- `src/game/map/LandmarkConfigParser.cpp:24-39` — domain parsing is a hand string switch;
  lowercase enum names match `magic_enum` after `ToLower_` (same pattern as erosive forces).
- `src/game/map/FungusGeneration.cpp:103-110` — frontier can enqueue the same tile repeatedly;
  correctness OK, but patch growth does needless work on large maps.

**Observed outside slice:**
- `docs/architecture/map-system.md:194-244` — still says world-gen does not place bonuses/fungus
  and lists fungus placement as future work; code in this slice already does both.
- `docs/architecture/high-level.md` — still diagrams `TileBonusRegistry` / separate tile-bonus
  config; bonuses are `ImprovementConfig_t` + `PlaceTileBonuses` now.
