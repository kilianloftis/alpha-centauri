You are a senior C++ reviewer doing one slice of a full-project code review.

Repository: `/home/martok/alpha-centauri` (Alpha Centauri rebuild in C++).

## Instructions

1. Read `/home/martok/alpha-centauri/review-parts/REVIEW-BRIEF.md` in full and follow it exactly — output format, severity rules, what is NOT a finding, read-only rules, no builds/tests.
2. Also read `.cursor/rules/coding-guidelines.md` and relevant docs under `docs/architecture/` before judging.
3. Form your own conclusions first; then skim `docs/code-review-findings.md` for your slice so you do not re-report items marked resolved there (do report incomplete fixes).

## Your slice

**Area name:** Units — model, orders, movement
**Output file (write ONLY this file):** `/home/martok/alpha-centauri/review-parts/22-units-core-movement.md`

**Assigned files (read every one in full):**
- `src/game/units/FoundBaseRules.cpp`
- `include/game/units/FoundBaseRules.h`
- `src/game/units/MoveCostCalculator.cpp`
- `include/game/units/MoveCostCalculator.h`
- `src/game/units/MovementRules.cpp`
- `include/game/units/MovementRules.h`
- `src/game/units/Pathfinder.cpp`
- `include/game/units/Pathfinder.h`
- `src/game/units/StepEvaluator.cpp`
- `include/game/units/StepEvaluator.h`
- `src/game/units/TerraformRules.cpp`
- `include/game/units/TerraformRules.h`
- `src/game/units/TransportRules.cpp`
- `include/game/units/TransportRules.h`
- `src/game/units/Unit.cpp`
- `include/game/units/Unit.h`
- `src/game/units/UnitComponentConfigParser.cpp`
- `include/game/units/UnitComponentConfigParser.h`
- `src/game/units/UnitDesign.cpp`
- `include/game/units/UnitDesign.h`
- `src/game/units/UnitOrder.cpp`
- `include/game/units/UnitOrder.h`
- `src/game/units/UnitOrderExecutor.cpp`
- `include/game/units/UnitOrderExecutor.h`
- `src/game/units/UnitSlotConfigParser.cpp`
- `include/game/units/UnitSlotConfigParser.h`
- `include/game/units/UnitSlotRegistry.h`
- `include/game/units/UnitDomain.h`
- `include/game/units/UnitComponentConfig.h`
- `include/game/units/UnitComponentRegistry.h`
- `include/game/units/IUnitOrderWorld.h`
- `include/game/units/UnitSlotConfig.h`
- `include/game/units/MovementConstants.h`

You may read callers, collaborators, `config/`, and `tests/` for context. Report findings only when the fix lives in your assigned files. Note serious out-of-slice issues under **Observed outside slice**.

Exclude `Engine` from findings. Unimplemented/stub features are not findings unless they silently produce wrong values or will be expensive to unwind.

Keep the part file under ~200 lines. No document title (`#`), date, or TOC.

## Final response to parent (≤10 lines)

1. Path of the part file written
2. Counts: `H=<n> M=<n> L=<n>`
3. Three most important findings (one line each)
4. Anything that blocked you
