# Thinker behaviour backlog

Notes from [induktio/thinker](https://github.com/induktio/thinker) (SMACX engine reconstruction) that are **not** in the Nutrients / Base wiki specs we treat as primary. Implement later when we want engine-parity quirks or optional rules.

Source files of interest: `src/base.cpp` — `mod_base_nutrient`, `mod_base_growth`.

## Nutrient growth / population limit

### Half nutrient tanks when full but at population limit

When `nutrients_accumulated >= nutrient_cost` but the base cannot grow (over hab complex / dome limit):

1. Set `nutrients_accumulated = nutrient_cost / 2`.
2. Still add this turn’s `nutrient_surplus` afterward.
3. Original UI shows a population-limit notice (`POPULATIONLIMIT`).

The [Nutrients wiki](https://alphacentauri.miraheze.org/wiki/Nutrients) does not describe this. Our shipping growth path leaves tanks unchanged on a blocked-full check (no grow, then deposit net and cap). Revisit if we want this penalty.

### No “net ≥ 0” gate on normal growth

Wiki: grow from a full tank only if net gain is neutral or positive.

Thinker: if tanks are full and growth is allowed, grow and empty even when `nutrient_surplus < 0`, then apply surplus (tanks can go negative immediately after).

We follow the wiki. Recorded here if we ever need a fidelity toggle.

### Starvation timing

Wiki: if net loss exhausts storage, population decreases as part of the opening upkeep beat.

Thinker: primarily starves when entering `mod_base_growth` with `nutrients_accumulated < 0` (often primed when surplus &lt; 0 and tanks were empty). If tanks were ≥ 0 and adding surplus would go negative, it often warns (`LOWNUTRIENT*`) and returns without starving that call.

Edge-case differences vs a strict wiki reading — revisit with playtesting.

### Recompute surplus after growth before deposit

After a successful grow, Thinker calls `base_compute(1)` so the surplus added that turn uses the **new** population’s intake. Our stage order (ResourceCollection before BaseGrowth) may approximate by subtracting one citizen’s intake from net when a pop is born; full recompute parity can land here later.

### Population boom path clamps

Under pop boom / Cloning Vats, Thinker may clamp `nutrients_accumulated` to `nutrient_cost` rather than emptying. Separate from normal growth; track with boom implementation.

## References

- Thinker `mod_base_growth` / `mod_base_nutrient` in `src/base.cpp`
- [Nutrients wiki](https://alphacentauri.miraheze.org/wiki/Nutrients) (primary for our growth plan)
- [Base wiki — Growth](https://alphacentauri2.info/wiki/Base.html)
