You are a senior C++ reviewer doing one slice of a full-project code review.

Repository: `/home/martok/alpha-centauri` (Alpha Centauri rebuild in C++).

## Instructions

1. Read `/home/martok/alpha-centauri/review-parts/REVIEW-BRIEF.md` in full and follow it exactly — output format, severity rules, what is NOT a finding, read-only rules, no builds/tests.
2. Also read `.cursor/rules/coding-guidelines.md` and relevant docs under `docs/architecture/` before judging.
3. Form your own conclusions first; then skim `docs/code-review-findings.md` for your slice so you do not re-report items marked resolved there (do report incomplete fixes).

## Your slice

**Area name:** Shared libraries
**Output file (write ONLY this file):** `/home/martok/alpha-centauri/review-parts/26-lib.md`

**Assigned files (read every one in full):**
- `src/lib/EventBus.cpp`
- `include/lib/EventBus.h`
- `src/lib/LuaRuntime.cpp`
- `include/lib/LuaRuntime.h`
- `src/lib/Rational.cpp`
- `include/lib/Rational.h`
- `src/lib/config/ConfigFields.cpp`
- `include/lib/config/ConfigFields.h`
- `include/lib/Signal.h`
- `include/lib/GameEvent.h`
- `include/lib/IdAllocator.h`
- `include/lib/RandomRoll.h`
- `include/lib/DerefView.h`
- `include/lib/Revision.h`
- `include/lib/Registry.h`
- `include/lib/config/JsonConfigLoader.h`

You may read callers, collaborators, `config/`, and `tests/` for context. Report findings only when the fix lives in your assigned files. Note serious out-of-slice issues under **Observed outside slice**.

Exclude `Engine` from findings. Unimplemented/stub features are not findings unless they silently produce wrong values or will be expensive to unwind.

Keep the part file under ~200 lines. No document title (`#`), date, or TOC.

## Final response to parent (≤10 lines)

1. Path of the part file written
2. Counts: `H=<n> M=<n> L=<n>`
3. Three most important findings (one line each)
4. Anything that blocked you
