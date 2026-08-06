## Map — runtime world model

**Files:** `src/game/map/TerraformSpread.cpp`, `include/game/map/TerraformSpread.h`,
`src/game/map/TerrainFeatureValidation.cpp`, `include/game/map/TerrainFeatureValidation.h`,
`src/game/map/TerritoryMap.cpp`, `include/game/map/TerritoryMap.h`, `src/game/map/Tile.cpp`,
`include/game/map/Tile.h`, `src/game/map/TileFlagMap.cpp`, `include/game/map/TileFlagMap.h`,
`src/game/map/TileLayerResolver.cpp`, `include/game/map/TileLayerResolver.h`,
`src/game/map/UnitPositionIndex.cpp`, `include/game/map/UnitPositionIndex.h`,
`src/game/map/WorkedTileIndex.cpp`, `include/game/map/WorkedTileIndex.h`,
`src/game/map/WorldMap.cpp`, `include/game/map/WorldMap.h`

**Assessment:** The occupancy indexes (`WorkedTileIndex`, `UnitPositionIndex`) are the
strongest part of this slice: RAII claims/registration, single writers for invariants, and
clear ownership under `WorldMap`. `Tile` as a plain data holder with mirrored terrain
feature pointers (plus `ValidateTerrainFeatures`) is coherent. The dominant weaknesses are
an ID-case bug in the layer resolver, a mutable escape hatch on the tile vector that can
dangle every `Tile*`-keyed index, and several silent no-ops where the project guidelines
require throws.

### [H] Query gameplay improvements with config ids, not sprite content ids
`src/game/map/TileLayerResolver.cpp:55-72` — `HasImprovement` is called with
`TileLayerContent::k_Farm` / `k_Forest` / `k_Road` (`"farm"` / `"forest"` / `"road"`), but
`config/improvements.json` ids are `"Farm"` / `"Forest"` / `"Road"`. Those checks always
fail, so Vegetation and Road layers never populate for real tiles. The skip list in
`ResolveImprovementLayer_` (`:86-88`) uses the same lowercase strings, so Farm/Forest/Road
fall through into the Improvement layer instead. Nothing calls `ResolveTileLayers` yet, but
the function is fully implemented and will mis-layer the first consumer; there are also no
tests. Direction: probe with the PascalCase improvement ids (or `magic_enum`/shared
constants) and keep returning the lowercase sprite content ids.

### [M] Stop exposing the owning tile vector as a mutable reference
`include/game/map/WorldMap.h:34-35` / `src/game/map/WorldMap.cpp:57-60` —
`GetTiles()` returns `std::vector<std::unique_ptr<Tile>>&`. Callers can `clear()`,
`reset()` elements, or reseat unique_ptrs while `UnitPositionIndex`, `WorkedTileIndex`,
bases, and units hold raw `Tile*` / `Tile&`. Address stability is load-bearing; this API
makes corruption a one-liner. Prior review §9 recorded this; it is still open. Direction:
expose a const span/range of `Tile&` (or const `unique_ptr` view) and keep mutation behind
world-gen/effects friends.

### [M] Make `TerritoryMap::Rebuild` fail loud on size mismatch
`src/game/map/TerritoryMap.cpp:116-121` — if unsized, `Rebuild` returns without updating
or throwing, so callers keep reading stale/`k_NoFactionOwner` as if ownership were current.
`ClaimFromBase_` (`:56-63`) indexes `visited` / `rBest` by base coordinates with no bounds
check against `m_width`/`m_height` and never asserts `rWorldMap` dimensions match — a
desynced `Reset` is undefined behavior on the origin write. Direction: throw unless
`IsSized()` and world width/height equal the grid; bounds-check the origin before indexing.

### [M] Do not swallow missing Forest/KelpFarm config during spread
`src/game/map/TerraformSpread.cpp:139-143` — `Find(improvementId)` returning null makes
`TrySpreadTerraformFromTile` return `false`, identical to “no eligible neighbor.” A modded
or incomplete `improvements.json` silently disables forest/kelp growth for the whole game.
`ValidateTerrainFeatures` does not cover these ids. Direction: use `Get` (throw) or validate
Forest/KelpFarm at load beside terrain enums.

### [M] Stop mutating fungus while probing spread eligibility
`src/game/map/TerraformSpread.cpp:89-100` — `PickBestSpreadNeighbor_` clears and restores
`SetHasFungus` on each candidate so `CanBuildImprovement` ignores the Fungus exclude. That
is a write in the middle of a selection pass (refreshes terrain-feature caches; any re-entrant
reader sees a lie), and it is not exception-safe if the check later grows throws. Direction:
test eligibility without touching tile state (e.g. treat Fungus as allowed for Forest spread
explicitly).

### [M] Reject non-positive `WorldMap` dimensions in the constructor
`src/game/map/WorldMap.cpp:8-21` — `width <= 0` or `height <= 0` yields an empty tile list,
`GetTile` always null (`:39-41`), and a zero-sized territory, with no throw. Guidelines
require constructors to produce valid objects. Direction: throw on non-positive dimensions.

### [L] Convention and hygiene items
- `src/game/map/TileLayerResolver.cpp:36-37` — `default:` on `Moisture_t` returns Arid and
  hides new enumerators from `-Werror=switch`.
- `include/game/map/Tile.h:3` — unused `#include <memory>`.
- `src/game/map/Tile.cpp:49-51` — empty user-declared destructor; prefer `= default` in the
  header if an out-of-line body is unnecessary.
- `include/game/map/Tile.h:79` — documents elevation range −4000…4000 but
  `SetElevation` (`Tile.cpp:95-98`) never enforces it.
- `src/game/map/TerraformSpread.cpp:79-107` — `bestScore` starts at `0` with `score > bestScore`;
  works for today’s ordinals but rejects a legitimate score of `0`; use a “found” flag or
  `INT_MIN`.
- `src/game/map/TileFlagMap.cpp:71-76` — out-of-bounds `Set` is a silent no-op (same pattern
  as `TerritoryMap::GetOwner`); prefer throw on unexpected coordinates once sized.
- `src/game/map/TileFlagMap.cpp:77` — stray trailing whitespace after the early-return brace.

**Observed outside slice:**
- `include/game/map/TileLayer.h:55-59` — lowercase `k_Farm`/`k_Forest`/`k_Road` are sprite
  content ids; the resolver bug above conflates them with improvement ids.
- `docs/architecture/map-system.md:117-120` — still describes
  `UnitPositionIndex::SetSingleUnitPerTile` / `TryMoveUnit`; stacking now lives in
  `MovementRules` and the API is `MoveUnit`.
- `docs/architecture/high-level.md` — still shows `TileMap` / `TileBonusRegistry` rather than
  the live `WorldMap` + indexes layout in `map-system.md`.
