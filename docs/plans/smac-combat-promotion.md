# SMAC combat promotion

Replace the continuous `U[0,1)` vs Lua `D/(A+D)` promotion chance with SMAC’s two discrete
`combat_rand` gates after a fight. Rules build the roll plan; mutations draw with
`RollExclusiveMax`.

## Target behaviour

Confirmed in `terranx.exe` / Thinker `veh_combat.cpp` (`promote` `0x5062B0`, outer gate
`0x50A250`–`0x50A42F`). `combat_rand(n)` ≡ `RollExclusiveMax(n, rng)`.

**Outer gate** (who may call `promote`):

| Survivor | Auto-pass | Else |
|----------|-----------|------|
| Attacker | effective morale ≤ Green (1) | `RollExclusiveMax(A+D) <= D` |
| Defender | effective morale ≤ Green | Only if the battle tile has a base; then `RollExclusiveMax(A+D) <= A`. If on base and effective morale ≥ Veteran (4), **no** promotion |

Artifact defender kills already skip via `KillEarnsPromotion` / `PromotionCandidate`.

**Inner `promote`:** if effective morale ≥ Elite, stop. If morale ≤ Disciplined, bump XP.
Otherwise bump only when `RollExclusiveMax(2) == 0`.

Use `CombatResult_t::attackStrength` / `defenseStrength` as A/D. Gates use **effective**
combat morale (SMAC `morale_veh`), not raw XP alone; success still does `SetXp(xp + 1)`.

## API

```cpp
struct CombatPromotionPlan_t
{
    bool bRun;
    bool bOuterAutoPass;
    int outerPool;       // A+D
    int outerThreshold;  // D attacker survivor, A defender
    bool bInnerAutoPass; // morale <= Disciplined
    bool bInnerCoinFlip; // above Disciplined, below Elite
};

CombatPromotionPlan_t BuildCombatPromotionPlan(
    const Unit& rSurvivor, CombatSide_t survivorSide, const CombatResult_t& rResult,
    const Tile& rBattleTile, const MoraleCalculator& rMorale, const EffectContext_t& rCtx);

bool TryPromote(Unit& rSurvivor, const CombatPromotionPlan_t& rPlan, std::mt19937& rRng);
```

- `PromotionCandidate` stays the “who may promote” filter; `BuildCombatPromotionPlan` adds
  base/Veteran/Green/Disciplined structure.
- `TryPromote` draws only when the plan requires it (no RNG when `!bRun` or auto-pass paths
  that skip a draw).
- Delete `MoraleCalculator::PromotionChance` and `ResolvePromotionChanceFromLevel`.

## Config

Remove from [`config/morale_levels.json`](config/morale_levels.json) and fixtures:

- `promotion_seed_formula`
- every level’s `promotion_chance` effects

Add required integer thresholds on the morale config (indices into level rows), e.g.
`promotion_green_max`, `promotion_disciplined_max`, `promotion_elite`,
`promotion_defender_base_skip_min` (shipping: 1, 2, 6, 4). Delete `StatId_t::PromotionChance`
and its wire string if unused elsewhere.

## Wiring

[`UnitAttackExecutor::ApplyFightAftermath_`](src/game/units/UnitAttackExecutor.cpp): after
`PromotionCandidate`, build the plan with the battle tile and call `TryPromote`. Probe
`TryPromoteProbeMission` stays unchanged (not combat promotion).

## Docs

- New subsection in [`docs/thinker/terranx-map.md`](docs/thinker/terranx-map.md): outer +
  inner promotion, addresses, defender-on-base and Veteran+ skip (index ≥ 4).
- Update [`docs/architecture/effects-system.md`](docs/architecture/effects-system.md) and
  unit-movement docs that describe continuous promotion chance.

## Tests

- Plan cases: Green outer auto-pass; Disciplined inner auto-pass; Hardened inner coin;
  Elite `bRun == false`; defender off-base `bRun == false`; defender on-base Veteran+ skip;
  attacker vs defender thresholds (D vs A) with the same A/D.
- `TryPromote` with fixed seeds: promote/skip and RNG consumption (no draw when `!bRun`).
- Replace continuous `PromotionChance` / Green-always / Elite-never calculator tests.
- Keep artifact “no promotion” via `PromotionCandidate`.
- Parser rejects leftover `promotion_seed_formula` / level `promotion_chance` through normal
  unknown-key / required-key rules (no migration shim).

## Files

- `MoraleRules`, `MoraleMutations`, `MoraleCalculator`, `MoraleConfig` + parser
- `UnitAttackExecutor.cpp`, `EffectEnums.h`
- `config/morale_levels.json`, fixtures, `MoraleCalculatorTests` / new rules tests
- Architecture + terranx-map docs
