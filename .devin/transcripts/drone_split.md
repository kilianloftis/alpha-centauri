# Dronecalculator raw stats
_Exported on 24/08/2026 at 20:22:07 GMT-7 from Cursor (3.8.11)_

---

**User**

Is dronecalculator collecting the raw drone stat from the effects system?

---

**Cursor**

I'll check how DroneCalculator gathers drone stats from the effects system.

**No — not inside `DroneCalculator` itself.** It only takes `DroneInputs_t` and feeds `resolved_drones` into the Lua formula.

The effects resolve happens in the `BaseManager` drone-input supplier:

```162:166:src/game/faction/base/BaseManager.cpp
        inputs.resolvedDrones = FinalizeResolvedStat(
            ResolveStatModifiers(
                FilterBaseLevelByStatId(rBaseEffects, StatId_t::Drones),
                static_cast<double>(inputs.baseSize))
                .total);
```

That is `StatId_t::Drones` resolved with **seed = base size** (so geometric multipliers like University scale pop), not a seed-0 “raw additive only” read.

`GetDroneModifier()` does resolve `Drones` with seed `0.0`, but composition still does not fold that (or away-from-home) into the calculator path — there’s an explicit TODO in `PopulationManager::RecalculateComposition`.

---

**User**

Is there a way we can use the same stat for population-based effects, like the University's faction bonus of 1 drone for ever 4 population, and raw adds, like Recreation Commons -2 drones?

---

**Cursor**

I'll check how `Drones` resolution and University / Recreation Commons effects are set up so we can see if one stat can cover both cases.

Yes — and the supplier already does that with one resolve of `StatId_t::Drones`.

Seed with **base size**, then let the modifier stack do the rest:

- **University** → `MultiplyGeometric` `1.25` (scales the size seed; ≈ +1 drone per 4 pop before `size_free_drones`)
- **Recreation Commons** → `Add` `-2` (flat on the same seed)

Stack order is Adds → AddPercents → MultiplyGeometrics, so for size 8 + Commons −2 + University ×1.25 you get `(8 - 2) * 1.25`, then the formula does `resolved_drones - size_free_drones`.

That is exactly this path:

```162:166:src/game/faction/base/BaseManager.cpp
        inputs.resolvedDrones = FinalizeResolvedStat(
            ResolveStatModifiers(
                FilterBaseLevelByStatId(rBaseEffects, StatId_t::Drones),
                static_cast<double>(inputs.baseSize))
                .total);
```

Do **not** use `GetDroneModifier()` / `SeedFor(Drones)` (seed `0`) for composition — that only sees flat Adds and zeros geometric population scaling. One seeded resolve is the combined answer; no second drone stat needed.

---

**User**

I don't think that gives us the values we want. A size 13 base with those two effects (excluding other contributions) should have 1 drone but the formula you describe results in 2 drones. The 1.25 is incorrect on it's own, it should be 0.25. Subtracting 2 from the base size also doesn't give the right amount of drones. And the multiplier has to be applied to the base size, not the base size modified by adds.

---

**Cursor**

You're right about the math — I'll confirm how the stack actually composes and what would yield 1 for that case.

You're right — that single-stack model is the wrong shape.

`ApplyModifierStack` is always `(seed + adds) * percents * geometrics`. For size 13, Commons `Add -2`, University `×0.25` on size alone, you need:

`floor(13 × 0.25) + (−2) = 1`

not `(13 − 2) × 0.25` (= 2 after finalize) or anything built on `×1.25`.

So for the same `Drones` stat to carry both, the resolve site has to **split** the ops:

1. **Pop-scaled** — multiplicative only, seed = `base_size` → `13 × 0.25`
2. **Flat** — additive only, seed = `0` → `−2`
3. **Sum** those (then finalize / feed the formula)

University’s config amount should be **`0.25`**, not `1.25`. And size drones can’t ride on “empty stack returns the seed” anymore — that only worked for the old ×1.25-as-scale-of-size idea. Size should stay `base_size - size_free_drones` (or equivalent) **outside** this effect term.

So: same stat, yes; one `ResolveStatModifiers(seed=baseSize)` call, no.

---

**User**

So how do we implement that without breaking other mechanics?

---

**Cursor**

I'll trace every consumer of `Drones` / `resolvedDrones` and the size-drone path so we can split the resolve without breaking those mechanics.

I have enough to lock a concrete approach: detach size drones from the `Drones` stack, and resolve that stack as mult@`base_size` + adds@0.

