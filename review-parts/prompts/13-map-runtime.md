You are a senior C++ reviewer doing one slice of a full-project code review.

Repository: `/home/martok/alpha-centauri` (Alpha Centauri rebuild in C++).

## Instructions

1. Read `/home/martok/alpha-centauri/review-parts/REVIEW-BRIEF.md` in full and follow it exactly — output format, severity rules, what is NOT a finding, read-only rules, no builds/tests.
2. Also read `.cursor/rules/coding-guidelines.md` and relevant docs under `docs/architecture/` before judging.
3. Form your own conclusions first; then skim `docs/code-review-findings.md` for your slice so you do not re-report items marked resolved there (do report incomplete fixes).

## Your slice

**Area name:** Map — runtime world model
**Output file (write ONLY this file):** `/home/martok/alpha-centauri/review-parts/13-map-runtime.md`

**Assigned files (read every one in full):**
- `src/game/map/TerraformSpread.cpp`
- `include/game/map/TerraformSpread.h`
- `src/game/map/TerrainFeatureValidation.cpp`
- `include/game/map/TerrainFeatureValidation.h`
- `src/game/map/TerritoryMap.cpp`
- `include/game/map/TerritoryMap.h`
- `src/game/map/Tile.cpp`
- `include/game/map/Tile.h`
- `src/game/map/TileFlagMap.cpp`
- `include/game/map/TileFlagMap.h`
- `src/game/map/TileLayerResolver.cpp`
- `include/game/map/TileLayerResolver.h`
- `src/game/map/UnitPositionIndex.cpp`
- `include/game/map/UnitPositionIndex.h`
- `src/game/map/WorkedTileIndex.cpp`
- `include/game/map/WorkedTileIndex.h`
- `src/game/map/WorldMap.cpp`
- `include/game/map/WorldMap.h`

You may read callers, collaborators, `config/`, and `tests/` for context. Report findings only when the fix lives in your assigned files. Note serious out-of-slice issues under **Observed outside slice**.

Exclude `Engine` from findings. Unimplemented/stub features are not findings unless they silently produce wrong values or will be expensive to unwind.

Keep the part file under ~200 lines. No document title (`#`), date, or TOC.

## Final response to parent (≤10 lines)

1. Path of the part file written
2. Counts: `H=<n> M=<n> L=<n>`
3. Three most important findings (one line each)
4. Anything that blocked you
