---
name: Combat strength from value and multiplier stats
overview: A combat rating is a value stat chosen by the kind of fight (conventional or psi) times a shared attack/defense multiplier, with tile_defense folded in at SMAC's multipliers and conditions. The rating is rounded down once at the 8.8 scale, then partial move and a new attack_handicap stat scale the attacker's whole-number strength in SMAC's order.
todos:
  - id: stats
    content: conventional_/psi_ attack and defense value stats; attack/defense become multipliers and reject Add; remove TileDefense; equipment readers move to the conventional stats
    status: completed
  - id: strength-resolve
    content: One ResolveCombatRating (value × multiplier × morale), FloorResolvedStat once; partial move on the whole number with the native exemption flag; attack_handicap after it
    status: completed
  - id: psi
    content: combat.json effects block; psi_attack / psi_defense Add effects; OpponentDomain condition
    status: completed
  - id: tiles
    content: Standing tile effects from the area collection; AttackerIsNativeLife and EffectIsUnowned conditions
    status: completed
  - id: config
    content: Equipment to conventional values; terrain / improvement multipliers at SMAC values; ability ops to MultiplyGeometric; Artifact clamps on both defense values; difficulty to attack_handicap; native partial-move flag
    status: completed
  - id: tests-docs
    content: Value stat, strength, partial move, handicap, psi and tile condition tests; fixtures; architecture docs; deviation entries
    status: completed
isProject: false
---

# Combat strength from value and multiplier stats

A combat rating is a value stat times a multiplier stat. The kind of fight picks the value
stat: `conventional_attack` / `conventional_defense` from weapons and armor, or `psi_attack` /
`psi_defense` from combat.json. `attack` and `defense` are multipliers that apply in both kinds
of fight, and every tile defense bonus is one of them or a conventional value multiplier. The
rating times morale is rounded down once at `k_CombatStrengthScale`. Partial move and the
difficulty handicap then scale the attacker's whole-number strength, each rounding down, in that
order.

## What the original does

From terranx-map.md "Combat strength and the round roll", "Combat odds" and "Battle panel":

- Strength is 8.8 fixed point: `rating × 8 × (morale + 6)`, with a further ×4 on both sides. A
  Disciplined rating-1 unit is 256, which is `k_CombatStrengthScale`. Each morale level is ⅛ of
  the Disciplined value: Green armor-1 is 224.
- Each percent bonus is its own multiply. Terrain and fortification are exact half-steps: ×1.5
  on rocky, on fungus and in a bunker, and ×2 on fungus for a native defender.
- In a psi fight, the Rules psi ratio replaces the weapon and armor value as the base. An attack
  rating reads the opponent's triad row, and a defense rating reads its own row. Shipping values
  are land 3:2, sea 1:1 and air 1:1. The battle panel shows the ratio on the base line, where a
  conventional fight shows the weapon or armor value.
- Tile bonuses:
  - **Rocky / fungus:** neither applies in psi, to an air defender, on a base tile or on ocean.
    Rocky takes precedence over fungus. Neither applies on fungus when the attacker is native.
  - **Base:** +25% in conventional and psi combat alike.
  - **Bunker:** neither in psi nor against an air attacker.
  - **Sensor:** +25% in either kind of fight. It needs the defender's faction to be a real
    faction, and a Sensor that is the defender's or stands in unowned territory.
- After the strength, three steps scale the attacker's whole number, each truncated:
  1. Partial move, `× moves / full move`. It is skipped for a wild attacker and for Mind Worms
     and Spore Launchers of any faction.
  2. Nerve gas.
  3. The difficulty handicap.

Within the strength calculation, the original truncates after every percent step. Rounding
down once leaves a 1-point gap when three or more 25% bonuses stack on an odd morale factor, or
when a Planet rating step comes before Empath Song. That residual is recorded as a deviation.

## Stats

| Stat | Kind | Holds |
| --- | --- | --- |
| `conventional_attack` | Additive | Weapon values; attack bonuses for conventional fights only |
| `psi_attack` | Additive | The psi ratio from combat.json; attack bonuses for psi fights only |
| `attack` | PureMultiplier | Attack bonuses for both kinds of fight |
| `conventional_defense` | Additive | Armor values; defense bonuses for conventional fights only |
| `psi_defense` | Additive | The psi ratio from combat.json; defense bonuses for psi fights only |
| `defense` | PureMultiplier | Defense bonuses for both kinds of fight |
| `attack_handicap` | PureMultiplier | The difficulty handicap, applied after partial move |

- **All Unit domain.** The four value stats and `attack_handicap` are new `StatId_t`
  enumerators with `ParseStatId` names, `KindFor` and `DomainFor` entries.
- **`Attack` and `Defense` become PureMultiplier** (seed 1).
- **No `Add` on a multiplier.** `EffectConfigParser` rejects `Add` on any PureMultiplier stat,
  so a flat value cannot land on `attack` or `defense`. No shipping config or fixture does this
  today.
- **Removed.** `StatId_t::TileDefense` goes from the enum, kind table, domain table and
  `"tile_defense"` parse, along with `TileEffectsContext::ResolveTileDefenseMultiplier`.
- **Equipment readers move to the conventional stats.** Each reads a weapon or armor value
  today:
  - `UnitDesign::IsCombatUnit` and `UnitDesign::FormatCombatRating`
    ([`UnitDesign.cpp`](src/game/units/UnitDesign.cpp)), and the same two in
    [`NativeDesign.cpp`](src/game/natives/NativeDesign.cpp).
  - [`UpgradeRules.cpp`](src/game/units/UpgradeRules.cpp) weapon and armour values.
  - [`UnitPlanRules.cpp`](src/game/units/UnitPlanRules.cpp) `PartAddAmount_`.
  - [`ScrambleRules.cpp`](src/game/combat/ScrambleRules.cpp) interceptor choice.
  - [`DesignStatsDisplay.cpp`](src/ui/unit-designer/DesignStatsDisplay.cpp).
  - The `IsCombatUnit_t` comment in [`EffectConfig.h`](include/game/effects/EffectConfig.h).
- **Morale levels.** [`MoraleConfigParser`](src/game/combat/MoraleConfigParser.cpp) rejects all
  six combat stats in level effects, as it rejects `attack` and `defense` today.

## Strength resolve

In [`CombatRules.cpp`](src/game/combat/CombatRules.cpp) `ResolveCombatStrength`:

```cpp
strength.bPsiCombat = IsPsiCombat_(rAttacker, rDefender);
const CombatValueStats_t& values = strength.bPsiCombat ? k_PsiValues : k_ConventionalValues;
const CombatRatingStats_t& defenderStats =
    engagement == CombatEngagement_t::ArtilleryDuel ? values.attack : values.defense;
const double attackRating = Rating_(attacker, values.attack);
const double defenseRating = Rating_(defender, defenderStats);

strength.attackStrength = FloorResolvedStat(attackRating * k_CombatStrengthScale);
strength.defenseStrength = FloorResolvedStat(defenseRating * k_CombatStrengthScale);
strength.attackStrength = PartialMoveAttack_(rAttacker, strength.attackStrength);
strength.attackStrength =
    FloorResolvedStat(strength.attackStrength * AttackHandicap_(attacker));
```

- **Rating stats.** `CombatRatingStats_t { StatId_t value; StatId_t multiplier; }` is declared in
  [`ActiveEffect.h`](include/game/effects/ActiveEffect.h). CombatRules holds
  `CombatValueStats_t { CombatRatingStats_t attack; CombatRatingStats_t defense; }` and its two
  constants:
  - `k_ConventionalValues`: `{ConventionalAttack, Attack}` and `{ConventionalDefense, Defense}`.
  - `k_PsiValues`: `{PsiAttack, Attack}` and `{PsiDefense, Defense}`.
- **One resolve function.** `ResolveCombatRating(unit, stats, ctx, moraleMultiplier)` replaces
  the two `moraleAddPercent` overloads. It collects the live unit's effects once and resolves:
  - the value stat from seed 0;
  - the multiplier stat from seed 1, with morale as one more `MultiplyGeometric` contribution.

  It returns their product. `PsiRating_` and `ConventionalRating_` collapse into one
  `Rating_`. The span overloads stay for MoraleRules, CollateralRules and NativeRules.
- **Ratings stay doubles.** Nothing rounds before the strength.
- **Morale is a multiply.** `MoraleCalculator::CombatAddPercent` becomes `CombatMultiplier`,
  returning `1 + percent / 100` (Green 0.875).
- **`FloorResolvedStat(double)`** sits beside `FinalizeResolvedStat`. It rounds down with a
  `1e-9` tolerance, so a product that is whole in decimal keeps its last point.
- **`PartialMoveAttack_`** applies when fragments remaining are below
  `k_moveFragmentsPerPoint`, unless the attacker resolves the `ignores_partial_move_penalty`
  rule flag. It computes `strength * remainingFragments / k_moveFragmentsPerPoint` in integers.
- **`AttackHandicap_`** resolves `attack_handicap` for the attacker in its fighter context.
- **Signature change.** `ResolveCombatStrength` drops its `TileEffectsContext` parameter, and
  [`CombatResolver`](src/game/combat/CombatResolver.cpp) updates its call.

## Psi

- **combat.json effects.** [`combat.json`](config/combat.json) gains an `effects` array. It loads
  the same way as [`world_rules.json`](config/world_rules.json):
  - `CombatConfigParser` accepts the key and parses it with `EffectConfigParser::ParseEffects`
    as `EffectSourceKind_t::WorldRules` under the source id `"combat"`, so world rules' scope
    rules apply. `CombatConfig_t` gains `std::vector<EffectConfig_t> effects`.
  - `GameState` appends them beside the world rules (`GameState.cpp`, `AppendActiveEffects`),
    so they reach every live unit through the faction pool.
  - `EffectReferenceValidator` checks them in `CheckRuleConfigSources_`.
  - The `WorldRules` enumerator comment names both files.
- **Psi value effects.** All four are `WorldGlobal` `Add`. Sea, air and orbital come out 1:1.

  | Stat | Amount | Condition |
  | --- | --- | --- |
  | `psi_attack` | 1 | none |
  | `psi_attack` | 2 | `OpponentDomain` land |
  | `psi_defense` | 1 | none |
  | `psi_defense` | 1 | `DefenderDomain` land |

  `OpponentDomain` gives the attacker the defender's row. In an artillery duel, it gives the
  defender, which rates on attack, the attacker's row.
- **New condition `OpponentDomain_t { std::vector<UnitDomain_t> domains; }`.** It is true when
  the other combatant's domain is in `domains`: `pDefender` when `combatRole` is Attacker,
  `pAttacker` when Defender. It fails closed with role None or without the other combatant, is
  situational, and the parser rejects an empty list.

## Tiles

- **Area collection for standing tile effects.** `AppendStandingTileUnitEffects_` collects from
  `CurrentGameState().GetTileEffects().CollectAreaEffects(tile)`, still keeping only Unit-domain
  stat modifiers. Radius effects such as Sensor reach the unit, and `owned_by_territory` owners
  are stamped on its own tile too.
- **New conditions.** Each gets a `ConditionVariant_t` alternative, a parser kind and an
  evaluator arm:
  - `AttackerIsNativeLife_t { bool bMatches = true; }` mirrors `DefenderIsNativeLife_t` on
    `pAttacker`. It fails closed without an attacker and is situational.
  - `EffectIsUnowned_t`: true when `effectOwnerFaction` is `k_NoFactionOwner`. It fails closed
    without a stamp and is situational.

## Config

Every SMAC percent step is a `MultiplyGeometric` factor. Conditions use `"present": false` /
`"value": false` for negation.

### Equipment

- [`weapons.json`](config/unit_components/weapons.json): each `attack` `Add` becomes a
  `conventional_attack` `Add`.
- [`armour.json`](config/unit_components/armour.json): each `defense` `Add` becomes a
  `conventional_defense` `Add`, including No Armour's 1.
- Psi Attack and Psi Armour keep only `forces_psi_combat`.

### Terrain and improvements

| Source | Stat | Factor | Condition |
| --- | --- | --- | --- |
| Rocky (`terrain.json`) | `conventional_defense` | ×1.5 | `DefenderDomain` land/sea; not `Base`; `AnyOf`[not `Fungus`, `AttackerIsNativeLife` false] |
| Fungus | `conventional_defense` | ×1.5 | `DefenderDomain` land/sea; not `Base`, `Rocky` or `Water`; `AttackerIsNativeLife` false; `DefenderIsNativeLife` false |
| Fungus | `conventional_defense` | ×2 | Same as above, with `DefenderIsNativeLife` true |
| Base (`improvements.json`) | `defense` | ×1.25 | none |
| Bunker | `conventional_defense` | ×1.5 | `AttackerDomain` land/sea |
| Sensor (radius 2, `owned_by_territory`) | `defense` | ×1.25 | `IsNativeLifeFaction` false; `AnyOf`[`SubjectIsOwner`, `EffectIsUnowned`] |

Rocky, Fungus and Bunker need no psi condition: a psi fight never reads
`conventional_defense`.

### Unit components

- **Converted to `MultiplyGeometric`, stats and conditions unchanged:**
  - AAA Tracking defense ×2.
  - Air Superiority attack ×2 against air or orbital, ×0.5 against land or sea.
  - Drop Pods attack ×0.5 after an airdrop.
  - Infantry attack ×1.25 against a Base.
  - Fungal Tower defense ×1.5.
- **Unchanged:** Bombard stays `AddPercent` on `attack` with `LevelsAboveOpponent`, as one step
  whose percent scales with the level difference.

### Alien Artifact

Its `MaxClamp` 1 and `MinClamp` 1 move from `defense` to `conventional_defense` and
`psi_defense`. A clamp caps only its own stat, so Rocky, Fungus and Bunker are capped. Base,
Sensor and morale sit on `defense` and now scale it; today the clamp also cancels morale.

### Difficulty

In [`difficulty.json`](config/difficulty.json), the handicap effects move from `attack` to
`attack_handicap` with the same amounts. They keep `PartnerPlayerType` and Talent's
`IsNativeLifeFaction`, and drop `IsDefending` false.

### Native units

- **Flag.** New rule flag `ignores_partial_move_penalty`.
- **[`native_units.json`](config/native_units.json):** Mind_Worm and Spore_Launcher carry it as
  a ThisUnit RuleFlag. Isle_of_the_Deep, Sea_Lurk, Locusts_of_Chiron and Fungal_Tower carry it
  with `IsNativeLifeFaction`.

## Tests

- **Value stats:**
  - A weapon's `conventional_attack` does not count in a psi fight, and a `psi_attack` `Add`
    does not count in a conventional one.
  - An `attack` multiplier applies in both kinds of fight.
  - The parser rejects `Add` on `attack`, `defense` and `attack_handicap`.
- **Strength:**
  - Disciplined armor-1 defends at 256 and Green armor-1 at 224.
  - A stack whose product is fractional rounds down once.
  - A 10%-step product that is whole in decimal keeps its last point.
- **Partial move:**
  - A 224 attacker with ⅓ move left strikes at 74. With the Specialist handicap (×1.5) it
    strikes at 111.
  - Mind Worms owned by a faction, and a wild Isle of the Deep, keep full strength.
  - A faction-owned Isle of the Deep is scaled.
- **Handicap:** a duel defender rating on attack does not pick up `attack_handicap`.
- **Psi:**
  - Disciplined units fight a land defender at 768 vs 512, and a sea defender at 256 vs 256.
  - In a duel, the defender's attack reads the attacker's domain.
  - `OpponentDomain` fails closed outside combat.
- **Tile bonuses:**
  - Rocky applies conventionally and not in psi, not for an air defender, not on a base tile,
    and not on rocky fungus against a native attacker.
  - Fungus gives ×1.5 normally, ×2 for a native defender, nothing against a native attacker,
    and nothing on water.
  - Base ×1.25 applies in psi.
  - Bunker does not apply against an air attacker or in psi.
- **Sensor:** it reaches a defender 2 tiles away; it applies for the owner and in unowned
  territory; it does not apply in foreign territory or to a wild native defender.
- **Alien Artifact:** its defense value is 1 in both kinds of fight; Rocky does not raise it
  and Base does.
- **Existing tests:**
  - The `ResolveTileDefenseMultiplier` assertions in
    [`TileEffectsTests`](tests/effects/TileEffectsTests.cpp),
    [`UniversalRoutingTests`](tests/effects/UniversalRoutingTests.cpp),
    [`BaseIntegrationTests`](tests/effects/BaseIntegrationTests.cpp) and
    [`CombatResolverTests`](tests/game/CombatResolverTests.cpp) become defender rating or
    strength assertions.
  - Tests that read weapon or armor values through `Attack` / `Defense` move to the
    conventional stats. Candidates: CollectionTests, FilterTests, ScrapUnitTests, AirdropTests,
    BombardTests, CombatRatingTests, ElevationChangeTests, LifetimeTransferTests,
    UnitPlanRulesTests and UnitStatsTests.
- **Fixtures:** `attack`, `defense` and `tile_defense` entries in `tests/fixtures/` follow the
  shipping mapping. Equipment `Add`s become conventional values, and tile bonuses go to the
  stat in the terrain and improvements table.
- **Parser and validation:** `ParserTests` and `ValidationTests` cover the four value stats,
  the kind change of `attack` / `defense`, `attack_handicap`, the three new conditions, the
  `Add` rejection and combat.json's `effects`. `ConfigStrictnessTests` cover the new
  combat.json key.
- **Morale:** `MoraleCalculatorTests` move to `CombatMultiplier`.

## Docs and deviations

- **[`effects-system.md`](docs/architecture/effects-system.md):**
  - The unit stats list and the Defense consumers describe value and multiplier stats, and
    `attack_handicap`.
  - The `TileDefense` passages (consumers, unit Defense vs TileDefense, the `ThisTile` scope
    line) go.
  - Morale is a combat multiplier, not part of the AddPercent bucket.
  - "Split into two StatIds when the quantities differ" uses `conventional_attack` vs
    `psi_attack` as its example.
  - The situational combat bonus example uses `MultiplyGeometric` 1.25.
  - The conditions list adds the three new conditions.
  - The world rules paragraph names combat.json's `effects` as a second `WorldRules` source,
    carrying the psi values.
- **[`research-system.md`](docs/architecture/research-system.md):** the world rules bullet
  names combat.json too.
- **[`unit-designer-system.md`](docs/architecture/unit-designer-system.md):** land design
  comparison reads `conventional_attack` and `conventional_defense`.
- **[`map-system.md`](docs/architecture/map-system.md):** the tile defense diagram nodes and
  bullets become ThisTile `conventional_defense` / `defense` effects reaching the unit through
  the area collection.
- **[`difficulty-system.md`](docs/architecture/difficulty-system.md):** the combat handicap is
  `attack_handicap`, applied after partial move.
- **[`docs/thinker/facilities.md`](docs/thinker/facilities.md):** the `tile_defense` and
  `ResolveTileDefenseMultiplier` mentions become `conventional_defense` / `defense`.
- **[`config/deviations/README.md`](config/deviations/README.md):**
  - "Combat strength scale and rounding" narrows to the in-strength truncation residual: off by
    1 when three or more 25% steps stack on an odd morale factor, or a Planet step precedes
    Empath Song.
  - Remove "Tile defense ignores combat context", "Psi strength ignores the triad ratio" and
    "Partial-move attacks penalize natives".
  - "Alien Artifact defends at full scale" notes that Rocky, Fungus and Bunker are capped by
    its clamp while Base, Sensor and morale scale it.
