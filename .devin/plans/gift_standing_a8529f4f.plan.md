---
name: Gift standing
overview: A talk proposal that gives without asking is a gift, and moves standing the way SMAC's make_gift does. DiplomacyLedger gains SMAC's per-pair gift count (diplo_gifts). Each offered item is valued before it is delivered. Credits lower the recipient's friction toward the giver by a tiered amount, and technology by the recipient's simple tech value. Both raise the giver's gift count, capped at the giver's blemishes. The allied-ground battle site floors the attacker's gift count at 1. Every gift's value passes through two faction-scope weights, one for the giver and one for the recipient, which a new GiftKind condition narrows to a kind; Morgan's credits and University's tech are effects on them. A third giver stat scales the thresholds a gift's size is tested against, which carries the Spartans' and Believers' base quirks. A base is valued with the built BaseValue. Trades that ask something back, base gift patience and the great satan stay recorded deviations.
todos:
  - id: re-record
    content: "terranx-map.md: new Gifts section (proposal-menu gift ids, make_gift energy / tech branches, gift count, treaty_off 0x60, replies, tech_val simple path, great_satan inputs), beside the existing Base gift and Base value sections; correct the communicate bullet (0x557A99 is SELFATROCITY)"
    status: completed
  - id: gifts-record
    content: DiplomacyLedger Get/SetGifts (directed); allied_territory_battle_gifts_floor and the floor in ApplyAlliedTerritoryFriction
    status: completed
  - id: gift-config
    content: diplomacy.json gifts block (credits, technology, base ladders and factors) + parser; Rational_t truncating scales; base_value factors; techs.json ai_weights + TechConfigParser
    status: completed
  - id: gift-weights
    content: TradeKind.h; StatId_t GiftGivenValue / GiftReceivedValue / GiftSizeThreshold; GiftKind condition (parser, context giftKind, evaluation, validator); University effects.json
    status: completed
  - id: gift-rules
    content: DiplomacyCalculator gift methods (base included); ResearchRules TechLevel; DiplomaticGiftRules (IsGift, GiftStanding for credits, technology and bases)
    status: completed
  - id: gift-mutations
    content: ApplyGiftStanding; ApplyProposal values each gift item before delivering it
    status: completed
  - id: deviations-docs
    content: Deviations entries; diplomacy-system.md gifts section, diagram and Not yet built; effects-system.md weights and GiftKind
    status: completed
  - id: tests-verify
    content: Ledger, parsers, GiftKind, calculator, rules, mutations, allied floor, talk popup mood; ./bd build; ./bd test; LastTest.log
    status: completed
isProject: false
---

# Gift standing

## Context

Part of [finish_diplomacy_roadmap_6ecba412.plan.md](.claude/plans/finish_diplomacy_roadmap_6ecba412.plan.md),
sections 1 and 2.
Talk can already send a trade with nothing asked in return. `ApplyProposal` moves the items and
writes no standing (TODO there; deviation "Talk trades write no standing" after this plan).

In SMAC the player's gift is `make_gift`, which `make_a_proposal` calls for the proposal-menu
item "a small token of my goodwill". `communicate`'s gift-count writes sit on branches where the
AI calls the player, which wait on AI initiation. Its blemish decrement at `0x557A99` is the
`SELFATROCITY` warning, not a gift.

Every AI still accepts (`EvaluateResponse_`).

## Confirmed in the binary

`terranx.exe` `sha1` `4b19c1fe3266b5ebc4305cd182ed6e864e3a1c4a`. Add to
[terranx-map.md](docs/thinker/terranx-map.md) as a "Gifts" subsection of "Diplomacy standing and
talk".

### The gift menu

`make_a_proposal(player, ai)` (`0x54F420`) sends proposal item 1 to `make_gift(player, ai)`
(`0x54F81A`). `proposal_menu` first asks `COUNTER1` ("You mean a gift? I like gifts.") over
`COUNTERMENU`, and stores the chosen id in `DiploCounterProposal` (`0x93FAB0`, `0x54EE5D`). For a
gift the menu offers: id 0 never mind; id 4 some research data, unless game rule `0x2000000`
(`0x54ED59`); id 7 all research data (`0x54ED96`); id 5 energy credits (`0x54EDD0`); id 8 a base,
when the player has more than one base in single player (`0x54EE18`–`0x54EE35`). `make_gift`
(`0x547C10`) branches on the id: 4 or 7 tech, 5 energy, anything else base.

Below, giver is the player and recipient the AI. "Key" is `MFaction.search_key` (`+0x1C`).

### Energy (`0x547C70`)

- `OFFERENERGY` lists amounts: the giver's credits rounded down to a multiple of 25, then halved
  and rounded down to 25 again, while non-zero (`0x547CD4`–`0x547D39`).
- The gift's value is the amount, doubled when the giver's key is `MORGAN`. A `SPARTANS` giver's
  value is doubled and then halved, so it is unchanged (`0x547DDE`–`0x547E49`).
- Credits are read before the transfer.
  - **Generous** when value ≥ recipient's credits / 2, ≥ giver's credits / 3, and ≥ 2 ×
    `CurrentTurn` (`0x547E62`–`0x547E9E`).
  - **Notable** when value ≥ recipient's credits / 4, ≥ 25, ≥ giver's credits / 6, and ≥
    `CurrentTurn` (`0x547EA7`–`0x547EE8`).
  - Otherwise small.
- Then the credits move (`0x547F35`–`0x547F46`).
- **Friction** (`0x547F6B`–`0x548025`):
  - units = value / 5;
  - sum = min(units, 10) / 2, plus each further block of up to 20 units divided by 3, then 4,
    then 5, and so on;
  - the recipient's friction toward the giver changes by −trunc(3 × ceil(sum / 5) / 2), clamped
    to 0–20 and mirrored into the cached `DiploFriction`, which is `cause_friction` written
    inline.
  - Worked values: under 10 credits 0; 10–25 → −1; 100 → −3; 200 → −4; 500 → −7.
- **Reply** `GAVEENERGY`. A generous gift says `GAVEENERGY1`, or `GAVEENERGY2` ("I may have to
  rethink my opinion of you!") when recipient→giver carries `0x60` or `great_satan(giver, 0)`.
  It also calls `treaty_off(giver, recipient, 0x60)` (`0x5480BD`).
- Gift count: generous or notable, below.

### Technology (`0x5488D2`)

- `tech_analysis(giver, recipient)` picks the tech the AI wants (`0x93FAA8`), and a second one
  (`0x93FA18`). With none, `REJTECHNOTHING`.
- **Id 4:** `OFFERTECH` names the first tech; `OFFERTECH2` also lets the player offer the
  second. Value = `tech_val(tech, recipient, 1)`, taken before `tech_achieved` (`0x548AFD`).
- **Id 7:** in id order, every tech the giver has and the recipient lacks adds its `tech_val` and
  is then delivered, so later values see earlier deliveries (`0x548984`–`0x548A00`).
- Value doubles when the recipient's key is `UNIV` or `SPARTANS`, then halves when it is
  `BELIEVE` (`0x548B50`–`0x548BA9`).
- **Friction:** the recipient's friction toward the giver changes by −value, clamped
  (`0x548BAC`–`0x548BF6`).
- **Generous** when value ≥ 16 (`0x548C14`):
  - the reply is `GAVETECH1`, or `GAVETECH2` when recipient→giver carries `0x60` and the cached
    `DiploFriction` is non-zero;
  - it calls `treaty_off(recipient, giver, 0x60)`.
- Otherwise the reply is `GAVETECH`, and the gift counts as notable.

### Gift count (`diplo_gifts[giver][recipient]`, `Faction+0x12C`)

- **Generous:** +1 if the count is below the giver's blemishes (`0x5480EF`). Then +1 again if it
  is still below them, or `great_satan(giver, 0)`, or multiplayer (`0x548124`–`0x548149`).
- **Notable:** only when the count is 0, the second step.
- **Small:** no change.

In single player, without the great satan, the count never passes the giver's blemishes.
`reputation(f, other)` reads it as `clamp(blemishes - diplo_gifts[other], 0, 99)`.

### Base (`0x548260`)

Recorded in [terranx-map.md](docs/thinker/terranx-map.md) "Base gift". The value is
`value_of_base` ("Base value"), built as `BaseValue` (`BaseValueRules`) by
[base_value_5b1e7c40.plan.md](.devin/plans/base_value_5b1e7c40.plan.md). Its skipped inputs
(fear, hostility, a scenario objective, a `BELIEVE` evaluator) are its own deviation, "Base value
skips the AI's attitude and scenario inputs".

The base branch has three faction quirks:
- a `BELIEVE` recipient doubles the value inside `value_of_base`, so every caller sees it;
- a `SPARTANS` giver doubles the value after friction is taken, so only the size test sees it;
- a `BELIEVE` giver is always generous.

### `tech_val(tech, faction, 1)` (`0x5BCBE0`)

Thinker's `mod_tech_val` asserts that its simple path equals this one.

1. value = Σ weight × (priority + 1) over power, tech, wealth and growth. Weights are the tech's
   `#TECHNOLOGY` columns 3–6 (power, tech, wealth, growth); priorities are the faction's 0/1 AI
   priorities.
2. Halve, rounding up (`0x5BCE42`), unless one of these matches:
   - the tech has power and the faction has the power priority or any Vendetta;
   - tech and tech;
   - wealth and wealth;
   - the tech has growth and the faction has the growth priority or fewer than 4 bases.
3. For an AI faction that lacks the tech:
   - add 2 − 2 × owners when more than one faction owns it (`0x5BCEC0`);
   - when the tech's level is below the highest level the faction knows (at least 1), scale by
     (level + 1) / (highest + 1);
   - floor at 1.
   - A tech's level is 1 plus its deepest prerequisite's (`tech_recurse`).

### Also recorded

- **`great_satan(f, 0)`** (`0x539C00`) holds only when:
  - f is human, ranked first, and has more than max(4, (turn + 25) / 50) bases;
  - difficulty and turn gates pass;
  - and then either the climactic battle makes f the target, or f's score beats the second
    faction's by a difficulty ratio.
  - There are no rankings or score here.
- **`treaty_off(a, b, mask)`** (`0x55B760`) clears mask both ways. A mask with `0x7` also clears
  `0x1100000`, and `0x10` also clears `0x8040`. `0x20` also clears agenda bits `0xC00`.
- **`SELFATROCITY`** (`0x557A59`): when the player's `atrocities` (`Faction+0x1D4`) is non-zero
  and no faction is its atrocity victim, the AI warns it once per pair (gated by `0x200000` /
  `0x400000`). The atrocity count and blemishes each drop by 1 while above 1. This corrects the
  map's `communicate` bullet.
- `communicate` also writes `diplo_gifts` at `0x553F0B`, `0x55518E`, `0x5551B3`, `0x555576` and
  `0x55568D`, on AI-initiated branches (not mapped).
- `tech_trade` writes friction after a swap (`0x542034`, not mapped). `energy_trade` writes none.

## Game layer

### The gift record

`DiplomacyLedger` gains `GetGifts(FactionId_t giver, FactionId_t recipient)` and
`SetGifts(giver, recipient, value)`, stored as `std::map<DirectedFactionPair, int> m_gifts`
(absent = 0). It is SMAC's `diplo_gifts[giver][recipient]`.

### Config

`config/diplomacy.json` and `tests/fixtures/diplomacy.json` gain the following, all required by
`DiplomacyConfigParser`:

```json
"allied_territory_battle_gifts_floor": 1,
"gifts": {
  "credits": {
    "sizes": [
      { "size": "Generous", "recipient_credits_factor": "1/2", "giver_credits_factor": "1/3", "turn_factor": 2, "minimum": 0 },
      { "size": "Notable", "recipient_credits_factor": "1/4", "giver_credits_factor": "1/6", "turn_factor": 1, "minimum": 25 },
      { "size": "Small" }
    ],
    "friction": {
      "credits_per_unit": 5, "first_block_units": 10, "first_block_divisor": 2,
      "block_units": 20, "units_per_point": 5, "factor": "3/2"
    }
  },
  "technology": {
    "sizes": [
      { "size": "Generous", "recipient_credits_factor": 0, "giver_credits_factor": 0, "turn_factor": 0, "minimum": 16 },
      { "size": "Notable" }
    ],
    "friction": { "factor": 1 },
    "value": { "priority_factor": 1, "unmatched_factor": "1/2", "growth_base_count": 4, "extra_owner_penalty": 2, "minimum": 1 }
  },
  "base": {
    "sizes": [
      { "size": "Generous", "recipient_credits_factor": "1/2", "giver_credits_factor": "1/3", "turn_factor": 2, "minimum": 0 },
      { "size": "Modest" }
    ],
    "friction": { "factor": "3/2" }
  }
}
```

`DiplomacyConfig_t` gains `alliedTerritoryBattleGiftsFloor` and `GiftConfig_t gifts`, made of
`CreditGiftConfig_t`, `TechGiftConfig_t` and `BaseGiftConfig_t`, keyed by trade kind.

- **Factors** are `Rational_t`: an integer or a fraction string. A product truncates, as SMAC's
  integer division does (`Rational_t::ScaledTruncated`); the unmatched tech factor rounds up
  (`ScaledRoundedUp`). A negative factor is rejected.
- **Size ladder** (`GiftSizesConfig_t`): rungs (`GiftSizeStepConfig_t`) tested in order, each
  with every threshold required, and a last entry with only a `size`, the fallback. `GiftSize_t`
  lives in `GiftSize.h`.
- **Friction:** each kind has a `factor`; credits apply theirs to the tiered points.
- `priority_factor` is tech_val's simple-path factor: each weight counts
  `priority_factor × priority + 1` times.
- `base_value`'s `ungarrisoned_divisor` and `outnumbered_divisor` become `ungarrisoned_factor`
  and `outnumbered_factor` (`"1/2"`).

**Tech AI weights.**
- Every entry in `config/techs.json` gains `"ai_weights": { "power", "tech", "wealth", "growth"
  }`, filled once from `alphax.txt` `#TECHNOLOGY` by tech name.
- `TechConfig_t` gains `TechAiWeights_t aiWeights`, and `TechConfigParser` requires it.
- The tech fixtures (`techs.json`, `techs_full.json`, `techs_commerce.json`) gain weights too.

### Gift weights

SMAC keys some quirks on the giver (Morgan's credits) and some on the recipient (University's
tech). So there is one weight per side, as `ProbeActorCost` and `ProbeActionCost` split a probe
quote, and a condition picks the kind.

- **Stats:**
  - `StatId_t::GiftGivenValue` is resolved on the giver's effects, and
    `StatId_t::GiftReceivedValue` on the recipient's.
  - Both are `RawScaled` in the `Faction` domain, with JSON names `gift_given_value` and
    `gift_received_value`.
  - A gift's raw value seeds the giver's resolve. That result, truncated, seeds the recipient's,
    whose result is truncated too.
  - `StatId_t::GiftSizeThreshold` (`gift_size_threshold`) is a `PureMultiplier` in the `Faction`
    domain, resolved on the giver. It scales every threshold the size test compares the value
    against, and leaves friction alone.
- **Context:**
  - `EffectContext_t` gains `std::optional<TradeKind_t> giftKind`.
  - Each resolve sets `pFaction` to the resolving faction, `pPartner` to the other side, and
    `giftKind` to the item's kind.
- **Condition:**
  - New `GiftKind_t { TradeKind_t kind; }`, written in JSON as
    `{ "kind": "GiftKind", "trade_kind": "Technology" }`.
  - `trade_kind` is parsed with `magic_enum` (case-insensitive) like `PartnerPlayerType`'s
    `type`, and an unknown kind throws.
  - It matches `giftKind` and fails closed without it.
  - It is situational (`ConditionNeedsSituationalContext`), and `EffectReferenceValidator`
    accepts it.
- **`TradeKind_t`:** moves from `TradeItem.h` to its own `include/game/diplomacy/TradeKind.h`,
  which `TradeItem.h` includes. That way `ActiveEffect.h` and `EffectConfig.h` can name it
  without `DiplomacyLedger.h`.
- **University:** `config/factions/university/effects.json` gains:

  ```json
  {
    "type": "StatModifier",
    "scope": "FactionGlobal",
    "condition": { "kind": "GiftKind", "trade_kind": "Technology" },
    "parameters": { "stat": "gift_received_value", "amount": 2, "op": "MultiplyGeometric" }
  }
  ```

- **SMAC's other quirks** wait on factions that don't ship yet:
  - Morgan: `gift_given_value` ×2 on `Credits`.
  - Spartans: `gift_received_value` ×2 on `Technology`, and `gift_size_threshold` ×0.5 on
    `Base`.
  - Believers: `gift_received_value` ×0.5 on `Technology` and ×2 on `Base`, and
    `gift_size_threshold` ×0 on `Base`.
  - A Spartans giver's credits are doubled and then halved, so they need no weight.
- **The Believers' ×2 on `Base`:**
  - It is `value_of_base`'s doubling for a `BELIEVE` evaluator. On a gift the evaluator is the
    recipient, so the weight matches.
  - Base swaps and threatened cession also call `value_of_base`, and there the Believers may be
    giving the base as well as receiving it.
  - When those are built, the doubling moves into a base value stat on the evaluator and comes
    off the gift weight.
- **The giver's base quirks** reach only the size test:
  - SMAC doubles a Spartans giver's base value after friction is taken. Testing the value against
    half of each threshold is the same test.
  - A Believers giver is always generous. With every threshold at 0, every value passes.
    `MultiplyGeometric` 0 follows Skunkworks on `PrototypeSurchargeScale`.

### Calculator

`DiplomacyCalculator`, from `gifts`:

- **Size methods** walk their kind's ladder and take the giver's `GiftSizeThreshold` as
  `thresholdScale`:
  - each threshold is first computed as its factor times the input, truncated as SMAC truncates;
  - it is then scaled, and the value is compared against the unrounded product.
- `GiftSizeInputs_t { value, recipientCredits, giverCredits, turn, thresholdScale }` carries a
  size's inputs.
- `CreditGiftSize`, `TechGiftSize` and `BaseGiftSize(const GiftSizeInputs_t&)`: the first rung
  reached, or the fallback.
- `int CreditGiftFriction(int value)`: the tiered points times the factor (≤ 0).
- `int TechGiftFriction(int value)` and `BaseGiftFriction(int value)`: the value times the
  factor, truncated (≤ 0).
- `int TechWeightValue(const TechAiWeights_t&, const AITendenciesConfig&, int vendettaCount,
  int baseCount)`: steps 1–2 of `tech_val`.
- `int UnownedTechValue(int value, int owners, int techLevel, int highestLevel)`: step 3.
- `int NextGifts(int current, int giverBlemishes, GiftSize_t)`: the count steps, capped at the
  giver's blemishes. `Modest` runs only the second step, at any count: every base gift runs it.
- `int AlliedTerritoryGifts(int current)`: `max(current, alliedTerritoryBattleGiftsFloor)`.

### Rules

`ResearchRules` gains `int TechLevel(const TechId&)`: 1 plus the deepest prerequisite's level
(the registry rejects cycles), and `int HighestTechLevel(const ResearchManager&)`: the deepest
level among the discovered techs, 0 with none. SMAC's floor of 1 on the highest level changes
nothing, since every tech is at least level 1.

New `DiplomaticGiftRules`:

- `bool IsGift(const DiplomaticProposal_t&)`: no requested status, nothing demanded, something
  given.
- `std::optional<GiftStanding_t> GiftStanding(FactionId_t giver, FactionId_t recipient, const
  TradeItem_t&)`. `GiftStanding_t` is `{ int frictionDelta; int gifts; }`: the change to the
  recipient's friction toward the giver, and the giver's next count toward the recipient. It
  reads the current state, so the caller asks before delivering the item.
  - Each kind's raw value below passes through both gift weights before friction or size reads
    it. Size is tested against thresholds scaled by the giver's `GiftSizeThreshold`.
  - **Credits:**
    - raw value = the amount;
    - size from both factions' current energy and `GetTurnNumber()`;
    - friction from `CreditGiftFriction`.
  - **Technology:**
    - raw value = `TechWeightValue` from the tech's weights, the recipient's `ai.json`
      priorities, its Vendetta count (factions with diplomacy) and its base count;
    - then `UnownedTechValue` when the recipient is AI-controlled and lacks the tech. Owners are
      the factions with diplomacy that have it; the highest level is the recipient's
      `HighestTechLevel`;
    - friction from `TechGiftFriction`, size from `TechGiftSize`.
  - **Base:**
    - raw value = `BaseValue(base, recipient, giver)`, read while the giver still holds it. The
      base is the giver's, so it is never the recipient's headquarters, which `BaseValue`
      rejects;
    - friction from `BaseGiftFriction`;
    - size from `BaseGiftSize` with both factions' current energy and `GetTurnNumber()`.
  - **Comm frequency, world map, declare Vendetta:** `nullopt`. SMAC has no gift rule for them.

### Mutations

- `DiplomaticStandingMutations` gains `ApplyGiftStanding(GameState&, FactionId_t giver,
  FactionId_t recipient, const TradeItem_t&)`. It applies `GiftStanding`: `ApplyFriction(recipient,
  giver, frictionDelta)` and `SetGifts(giver, recipient, gifts)`.
- **`ApplyProposal`:**
  - When `IsGift`, each given item gets `ApplyGiftStanding`, then is delivered, in order.
  - Otherwise items move as now.
  - The TODO narrows to trades.
- **`ApplyAlliedTerritoryFriction`:** also sets the attacker's count toward the owner to
  `AlliedTerritoryGifts`. `AlliedTerritoryOwner`'s TODO keeps only the +3 refused-demand site,
  which waits on AI-initiated `#DEMANDATTACK`.

Talk needs no change. Its trade flow already sends gifts through `Propose`, and the portrait's
mood reads the lowered friction.

## Deviations

[config/deviations/README.md](config/deviations/README.md):

- **"Talk gifts and trades write no standing"** becomes "Talk trades write no standing". Original:
  `tech_trade` moves friction after a swap (`0x542034`). Missing: the RE of that delta and the
  writer.
- **New "Base gifts skip patience":**
  - Original: the recipient's `diplo_patience` toward the giver drops by 2, or 3 when generous
    ("Base gift").
  - Ours: friction, size and the gift count follow the binary, but there is no patience record.
  - Where: `GiftStanding`.
  - Missing: a patience record.
- **"Base value skips the AI's attitude and scenario inputs"** gains: on a base gift, the
  doubling for a `BELIEVE` evaluator is the Believers' `gift_received_value` weight on `Base`
  (Gift weights). Base swaps and threatened cession still need a base value stat, which then
  takes the doubling over from the gift weight.
- **New "Gift count ignores the great satan":**
  - Original: the second step also passes the giver's blemishes for the great satan or in
    multiplayer.
  - Ours: always capped.
  - Where: `NextGifts`.
  - Missing: rankings, score and `great_satan`.
- **New "Generous gifts":**
  - Original: a generous gift, base gifts included, clears revenge both ways (`treaty_off`
    `0x60`). Energy and tech reply `GAVEENERGY1` / `GAVETECH1` / `…2`.
  - Ours: there is no revenge record, and the reply is `talk.json`'s trade acceptance.
  - Where: `ApplyProposal`, `config/ui/talk.json`.
  - Missing: the revenge record and its clearing, a gift result reported to talk, and the
    `GAVE*` lines.
- **"Trade proposals bundle items `[Ours]`"** gains:
  - each offered item of a gift is valued as its own `make_gift`, in order;
  - the player picks any tech, where SMAC's AI names one or two;
  - there is no "all research data";
  - credit amounts come from `credit_offers`, where `make_gift` offers the giver's credits
    rounded to 25 and halving.

## Docs

- [terranx-map.md](docs/thinker/terranx-map.md): the Gifts subsection above, and the
  `communicate` bullet corrected.
- [diplomacy-system.md](docs/architecture/diplomacy-system.md):
  - a "Gifts" subsection under standing: the record, `IsGift`, `GiftStanding`, the per-item
    order, and the two gift weights;
  - `DiplomaticGiftRules` in the diagram;
  - "Not yet built" and "Talk standing" updated to cover trades, base gift patience, the great
    satan, and the revenge record.
- [effects-system.md](docs/architecture/effects-system.md):
  - `GiftGivenValue` and `GiftReceivedValue` in the stat list (RawScaled, Faction; giver then
    recipient), and `GiftSizeThreshold` (PureMultiplier, Faction; the giver's size thresholds);
  - `GiftKind_t` in the condition list, on `EffectContext_t::giftKind`.

## Tests

Fixtures own every number. Neither a test nor a rule reads `config/`.

- **`DiplomacyLedgerTests`:** gifts are directed and start at 0.
- **`DiplomacyConfigParserTests`:** the `gifts` block and the floor are required; every rung
  but the last needs each threshold and the last has none; factors are integers or fraction
  strings, never negative; `base_value` reads its two factors.
- **`Rational_t`:** `ScaledTruncated` truncates toward zero, `ScaledRoundedUp` rounds up, and
  both report overflow.
  **`TechConfigParser` tests:** `ai_weights` is required.
- **Effects:**
  - `EffectConfigParser`: `GiftKind` parses each trade kind and rejects an unknown one.
  - Condition evaluation: `GiftKind` matches its kind, misses another, and fails closed without
    `giftKind`.
- **`DiplomacyCalculatorTests`:**
  - `CreditGiftFriction`: 9 → 0, 10 → −1, 25 → −1, 100 → −3, 200 → −4, 500 → −7.
  - `CreditGiftSize` at each threshold's edge.
  - `TechGiftSize` at 15 and 16, whatever the treasuries; `TechGiftFriction` is −value.
  - `NextGifts`: generous +2 under the cap; generous +1 one below it; notable only from 0; modest
    +1 at a non-zero count; small none; 0 blemishes never counts.
  - `BaseGiftFriction` and `BaseGiftSize` at each threshold's edge.
  - Threshold scale: at 0.5 an odd threshold compares unrounded (threshold 3 → 1.5, so value 1
    fails and 2 passes); at 0 every value is generous.
  - `TechWeightValue`: a match keeps the sum; no match halves, rounding up; growth matches below
    4 bases.
  - `UnownedTechValue`: the owner penalty, level scaling, the floor of 1.
- **`ResearchRules`:** `TechLevel` of a root, a chain, and two prerequisites of different depth.
- **New `DiplomaticGiftRulesTests`:**
  - **Credits:**
    - lowers the recipient's friction toward the giver, sized from credits held before the
      transfer;
    - a giver whose `gift_given_value` is ×2 on `Credits` doubles the value, and its ×2 on
      `Technology` leaves credits alone.
  - **Technology:**
    - lowers friction by its value;
    - a recipient whose `gift_received_value` is ×2 on `Technology` doubles it. The same effect
      on the giver doesn't;
    - an owner already holding it lowers it.
  - **Weights:** a giver ×2 and a recipient ×0.5 on the same kind leave an odd raw value
    truncated after each step.
  - **Base:**
    - lowers friction by `BaseGiftFriction` of its `BaseValue`;
    - generous from credits;
    - a modest gift's count step;
    - a giver whose `gift_size_threshold` is ×0.5 on `Base` is generous at half the thresholds,
      with friction unchanged. At ×0 every base gift is generous.
  - **No standing:** comm frequency, map and Vendetta gifts; a trade that asks something back; a
    proposal with a status.
- **`DiplomaticStandingMutationsTests`:**
  - `ApplyGiftStanding` writes friction and the count;
  - two credit items are valued in order, the second after the first moved;
  - a base is valued while the giver still holds it;
  - `ApplyAlliedTerritoryFriction` floors the count at 1 and keeps a higher one.
- **`TalkPopupTests`:** offering credits and choosing "Deal." moves the rival's credits and lowers
  the mood shown on the portrait.

## Verification

`./bd build`, `./bd test`, and check `build/Testing/Temporary/LastTest.log` for the new cases.
Boot the game to confirm that `diplomacy.json`, `techs.json` and University's `effects.json`
load.
