# Thinker behaviour backlog

Notes from [induktio/thinker](https://github.com/induktio/thinker) (SMACX engine reconstruction). Confirmed binary behaviour is in `terranx-map.md` (`base_nutrient`, `base_growth`, pod pop cost).

Source files of interest: `src/base.cpp` — `mod_base_nutrient`, `mod_base_growth`.

## Nutrient growth / population limit

### Half nutrient tanks when full but at population limit

When `nutrients_accumulated >= nutrient_cost` but the base cannot grow (over hab complex / dome limit):

1. Set `nutrients_accumulated = nutrient_cost / 2`.
2. Still add this turn’s `nutrient_surplus` afterward.
3. Original UI shows a population-limit notice (`POPULATIONLIMIT`).

**Implemented** in `ResolveStalled_` (`GrowthRules`). The `POPULATIONLIMIT` notice remains deferred.

### No “net ≥ 0” gate on normal growth

Wiki: grow from a full tank only if net gain is neutral or positive.

Binary: if tanks are full and growth is allowed, grow and empty even when `nutrient_surplus < 0`, then apply surplus (tanks can go negative immediately after).

**We follow the wiki** (`IsNutrientBoxFull_` requires `net >= 0`). Recorded here if we ever need a fidelity toggle.

### Starvation timing

Two stores matter:

- **Resource nutrient bank** — this turn’s gross tile nutrients (`ResourceManager`), filled in
  `ResourceCollection` and drained in `BaseGrowth`.
- **Nutrient tanks** — the persistent growth stockpile (`PopulationManager`). Priming and
  deposit change only this.

Priming is **not** a transfer from the bank into the tanks. It only rewrites tank state using
this turn’s net (gross − citizen intake) as a signal, before production runs:

| Current tanks | This turn’s net | After priming |
|---------------|-----------------|---------------|
| negative      | ≥ 0             | 0             |
| exactly 0     | < 0             | −1            |
| anything else | (any)           | unchanged     |

Turn order (binary and ours):

1. **ResourceCollection** — fill the bank; run `PrimeNutrientTanks` (signal only).
2. **BaseProduction** — finishing a colony pod (or any `ModifyPopulation` shrink) first sets
   tanks to **0 if they were negative**, then removes the citizen(s). That wipe is what
   “clamps to 0, then pops” means: clear a −1 starvation mark so growth will not treat the
   base as already starving after the pop cost.
3. **BaseGrowth** — `ConsumeNutrients` drains the bank into `ResolveGrowth` as gross production.
   Starvation fires only if tanks are already `< 0` on entry. Otherwise boom / near-zero /
   threshold grow / deposit run; deposit may leave tanks negative for a **later** turn.

Why priming cannot live only inside `ResolveGrowth`: growth runs after production. A size-2
base with empty tanks and a food deficit must become −1 **before** the pod completes, so the
pod can clear that −1 to 0. If priming waited until growth, the leftover size-1 base would
be primed to −1 and starve in the same turn — harsher than SMAC.

**Implemented:** `PrimeNutrientTanks` after `ProduceResources`; stockpile wipe on
`ModifyPopulation` shrink; `ResolveStarvation_` when `stockpile < 0`; deposit upper-clamps
only (no floor at 0).

### Recompute surplus after growth before deposit

After a successful grow, binary recomputes so the surplus uses the new population’s intake. We approximate by subtracting one citizen’s intake on the threshold-growth path; full recompute parity can land later.

### Population boom / near-zero

Under pop boom (Growth ≥ 6) or Cloning Vats, a base whose surplus is at least one citizen's intake grows every turn regardless of tank level. Tanks are not emptied: they are clamped down to the pre-growth `nutrient_cost` if above it, and that turn's surplus is not added. At Growth ≤ −3 full tanks are pinned to `nutrient_cost` and the base never grows from tanks. Confirmed in the binary; consumed via `PopulationBoom` / `NearZeroGrowth` rule flags in `ResolveGrowth`.

## References

- Thinker `mod_base_growth` / `mod_base_nutrient` in `src/base.cpp`
- `docs/thinker/terranx-map.md` (`base_nutrient`, `base_growth`, pod pop cost)
- [Nutrients wiki](https://alphacentauri.miraheze.org/wiki/Nutrients)
- [Base wiki — Growth](https://alphacentauri2.info/wiki/Base.html)
