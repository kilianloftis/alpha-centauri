You are a senior software architect reviewing the higher-level architecture of this project — not line-by-line code style.

Repository: `/home/martok/alpha-centauri` (Alpha Centauri rebuild in C++).

## Scope

Examine how major components fit together: boundaries, dependency direction, ownership, extension points (config/Lua/modding), and whether the documented architecture matches reality.

Read:
- `docs/architecture/high-level.md` and every other file in `docs/architecture/`
- `.cursor/rules/architecting.md` and `.cursor/rules/coding-guidelines.md` (architecture-relevant parts)
- Composition roots and wiring: `include/game/GameState.h`, `src/game/GameState.cpp`, `include/game/GameDataContext.h`, `src/game/GameDataContext.cpp`, `include/game/Faction.h`, `src/game/TurnProcessor.cpp`, `include/game/TurnStageFactory.h`, `include/ui/UIManager.h`, `include/ui/ViewFactory.h`, `include/lib/EventBus.h`, `include/lib/Signal.h`, `include/game/IEffectsProvider.h`, `include/game/HookContext.h`
- Skim subsystem entry points as needed (faction, map, units, council, effects, stages, UI) to validate boundaries — do not do a full code review of those files (other agents own that).
- Skim `docs/code-review-findings.md` for architecture items already resolved.

Exclude `Engine` (ad-hoc testing catchall). Many systems are intentionally unimplemented — missing features are not findings; mis-wired stubs that create wrong runtime semantics or paint the architecture into a corner are.

## What to judge

1. Component boundaries and SRP at the *subsystem* level
2. Dependency direction / layering violations (UI→game ok; game→UI not; lib purity)
3. Ownership and lifetime (who owns WorldMap, Faction managers, registries, effects pools)
4. Extension/moddability seams (config registries, Lua hooks, turn stages) vs hardcoded coupling
5. Consistency between `docs/architecture/*` and the actual wiring
6. Cross-cutting concerns (events/signals, effects aggregation, turn pipeline) — clarity and fragility
7. What will hurt most as unimplemented systems are filled in

## Output

Write ONLY `/home/martok/alpha-centauri/review-parts/00-architecture.md` using this structure (no `#` title):

```markdown
## Architecture

**Scope:** high-level component structure (not a per-file code review)

**Assessment:** 3–6 sentences on overall architectural health and the dominant structural risk.

### [H] ...
### [M] ...
### [L] ...
```

Cite evidence with `path:line` or doc paths. Order [H] then [M] then [L]. Under ~200 lines. No builds/tests. Read-only except your part file.

## Final response to parent (≤10 lines)

1. Path written
2. Counts H/M/L
3. Three most important architectural findings
4. Blockers if any
