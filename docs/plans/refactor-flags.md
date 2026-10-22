# Refactor flags

Rule violations found during the [readable-code cleanup](readable-code-cleanup.md) that need
more than a local fix. Each one gets a dedicated plan later. Until then the code stays as it is.

One entry per violation, under the rule it breaks:

```markdown
### `Faction::CreateBase` — `bMayOccupyWater`
- **Where:** `include/game/faction/Faction.h:136`; callers in `UnitOrderExecutor.cpp`,
  `Engine.cpp` and tests
- **Problem:** a defaulted flag chooses whether the new base may sit on water.
- **Why not local:** `UnitOrderExecutor` computes it with `FoundedBaseMayOccupyWater(rUnit)`;
  deriving it inside `CreateBase` needs the founding unit, which the other callers do not have.
```

## Rule 1: one call deep

## Rule 3: impossible-case guards

## Rule 4: pass-through functions

## Rule 5: flag parameters

## Rule 6: speculative code

## Rule 7: long functions
