# SMAC combat promotion (eligibility × success)

Match SMAC’s promotion **rates** with one continuous `U[0,1)` roll. Confirmed in
`terranx.exe` (`promote` `0x5062B0`, outer gate `0x50A29E`–`0x50A42A`); see
[`docs/thinker/terranx-map.md`](../thinker/terranx-map.md) “Combat promotion”.

## Target rates

Gates use **effective** morale (`EffectiveMoraleLevel`). Success still `SetXp(xp + 1)`.
Final chance = `promotion_eligibility_chance × promotion_success_chance`.

| Effective morale | Eligibility (attacker / field defender) | Success |
|------------------|------------------------------------------|---------|
| ≤ Green (1) | 1 | 1 |
| Disciplined (2) | side ratio | 1 |
| Hardened (3) | side ratio | 0.5 |
| Veteran–Commando | side ratio | 0.5 |
| Elite (6) | 0 | 0 |

Side ratio seed: `their_strength / (our_strength + their_strength)` from the survivor's
point of view (attacker: our=A their=D; defender: our=D their=A).
On-base defender with effective morale ≤ Hardened: Base `ThisTile` MinClamp eligibility to 1.

## Shape

- Stats: `PromotionEligibilityChance`, `PromotionSuccessChance` (replace `PromotionChance`).
- Root `promotion_eligibility_formula` (Lua: `our_strength`, `their_strength`). Success
  seeds at 1.0 in resolve. Level rows only add modifiers that differ from identity
  (Green eligibility MinClamp, Hardened+ success ×0.5, Elite eligibility 0).
- Base improvement `ThisTile` MinClamp on eligibility (`IsDefending` + `EffectiveMorale` Le 3);
  reaches the unit via `CollectLiveUnitEffects` like any other unit-domain source.
- `CombatPromotionChance` uses `ResolveCombatUnitMultiplicativeStat`; `TryPromote` rolls once.
