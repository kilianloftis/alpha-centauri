You are a senior C++ reviewer doing one slice of a full-project code review.

Repository: `/home/martok/alpha-centauri` (Alpha Centauri rebuild in C++).

## Instructions

1. Read `/home/martok/alpha-centauri/review-parts/REVIEW-BRIEF.md` in full and follow it exactly — output format, severity rules, what is NOT a finding, read-only rules, no builds/tests.
2. Also read `.cursor/rules/coding-guidelines.md` and relevant docs under `docs/architecture/` before judging.
3. Form your own conclusions first; then skim `docs/code-review-findings.md` for your slice so you do not re-report items marked resolved there (do report incomplete fixes).

## Your slice

**Area name:** Map — world generation and config
**Output file (write ONLY this file):** `/home/martok/alpha-centauri/review-parts/14-map-generation.md`

**Assigned files (read every one in full):**
- `src/game/map/FbmNoise.cpp`
- `include/game/map/FbmNoise.h`
- `src/game/map/FungusGeneration.cpp`
- `include/game/map/FungusGeneration.h`
- `src/game/map/ImprovementConfigParser.cpp`
- `include/game/map/ImprovementConfigParser.h`
- `src/game/map/LandmarkConfigParser.cpp`
- `include/game/map/LandmarkConfigParser.h`
- `src/game/map/LandmarkGeneration.cpp`
- `include/game/map/LandmarkGeneration.h`
- `src/game/map/MapGenerationConfig.cpp`
- `include/game/map/MapGenerationConfig.h`
- `src/game/map/RiverGeneration.cpp`
- `include/game/map/RiverGeneration.h`
- `src/game/map/TileBonusGeneration.cpp`
- `include/game/map/TileBonusGeneration.h`
- `src/game/map/WorldGenDecorationConfigParser.cpp`
- `include/game/map/WorldGenDecorationConfigParser.h`
- `src/game/map/WorldGenPresetConfigParser.cpp`
- `include/game/map/WorldGenPresetConfigParser.h`
- `src/game/map/WorldGenerator.cpp`
- `include/game/map/WorldGenerator.h`
- `include/game/map/MapUtils.h`
- `include/game/map/WorldGenPresetRegistry.h`
- `include/game/map/WorldGenDecorationConfig.h`
- `include/game/map/MoistureGeneration.h`
- `include/game/map/RockinessGeneration.h`
- `include/game/map/TileLayer.h`
- `include/game/map/ImprovementRegistry.h`
- `include/game/map/LandmarkConfig.h`
- `include/game/map/WorldGenPresetConfig.h`

You may read callers, collaborators, `config/`, and `tests/` for context. Report findings only when the fix lives in your assigned files. Note serious out-of-slice issues under **Observed outside slice**.

Exclude `Engine` from findings. Unimplemented/stub features are not findings unless they silently produce wrong values or will be expensive to unwind.

Keep the part file under ~200 lines. No document title (`#`), date, or TOC.

## Final response to parent (≤10 lines)

1. Path of the part file written
2. Counts: `H=<n> M=<n> L=<n>`
3. Three most important findings (one line each)
4. Anything that blocked you
