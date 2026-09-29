---
name: ModifyCounter
overview: "A triggered effect that writes a stored per-faction tally (`CounterId_t`), read by C++ rules. The first counter is SMAC's `mind_control_total`: paying for Mind Control or Total Thought Control adds 4, a successful Subvert Unit adds 1, and the acting faction's total raises the base mind-control cost."
todos:
  - id: counter-enum
    content: CounterId_t (MindControlTotal) + ParseCounterId in EffectEnums.h
    status: completed
  - id: faction-store
    content: Faction counter array + GetCounter / SetCounter
    status: completed
  - id: effect-arm
    content: ModifyCounterEffect_t, ParseModifyCounter_, ApplyOne_ arm, IsPerFactionSubject_, CounterModified_t, EffectReferenceValidator no-op
    status: completed
  - id: probe-paid-list
    content: on_paid_effects on probe actions (rejected without cost); ApplyProbePaidEffects fired after payment, before the roll
    status: completed
  - id: probe-success-list
    content: Subvert Unit runs on_success_effects; parser rejects on_success_effects on handlers that never run them
    status: completed
  - id: probe-config
    content: Author ModifyCounter entries and mind_control_divisor in config and fixture
    status: completed
  - id: probe-reader
    content: QuoteProbeActionCost takes the actor; mind_control_divisor required on base-target costs; actor total / divisor in the first factor
    status: completed
  - id: tests
    content: Parser, dispatch, probe writer and cost tests; ./bd test
    status: completed
  - id: docs
    content: effects-system.md (triggered list, Counters section, trigger-slot table); TriggeredEffect.h, ProbeActionConfig.h and ProbeRules comments
    status: completed
isProject: false
---

# ModifyCounter

## What it is

A **counter** is a stored integer that records a fact, such as "how much mind control this
faction has done". A triggered effect writes it, and C++ rules read it. It is neither of the
things it resembles:

- **A stat** (`StatId_t`) is derived: it is resolved from continuous effects and has no storage,
  so triggers never write one.
- **A `oncePer` key** is an open string that only the dispatcher reads.

A counter's weight belongs to the rule that reads it (in config). The save holds only the count.

```json
{ "type": "ModifyCounter", "parameters": { "counter": "mind_control_total", "amount": 4 } }
```

## Design

**`CounterId_t`**
- A closed enum in `EffectEnums.h`, beside `StatId_t` and `RuleFlagId_t`, with a
  `ParseCounterId` that mirrors `ParseStatId`: snake_case wire forms, throwing on an unknown id.
- It grows only when a reader is written. The first entry is `MindControlTotal`
  (`mind_control_total`).

**Storage on `Faction`**
- A zero-initialized `std::array<int, magic_enum::enum_count<CounterId_t>()>` beside
  `m_consumedTriggerKeys`, with `GetCounter(CounterId_t)` and `SetCounter(CounterId_t, int)`.
- It is save data, so it goes wherever `Faction` serializes once serialization is wired.

**`ModifyCounterEffect_t { CounterId_t counter; int amount; }`**
- **Parser.** `ParseModifyCounter_`:
  - `counter` is required and goes through `ParseCounterId`.
  - `amount` is required and must be a positive integer. Check `is_number_integer()`, as
    `ParseDestroyFacility_` does for `count`; `RequireNumber` returns a double, and the cast
    would truncate 1.5 to 1.
  - Register it in the parser's type table. The continuous parser then rejects it
    automatically.
- **Dispatch.** It is a per-faction subject, like `GrantEnergy`: it applies once to each
  faction in `context.factions`, never to the context base's owner. In a probe mission the
  actor is in `factions` while `pBase` and `pFaction` name the victim. The `ApplyOne_` arm:
  - calls `SetCounter(counter, GetCounter(counter) + amount)`
  - pushes `CounterModified_t { counter, amount }`
  - returns `true`
- **Validator.** `EffectReferenceValidator`'s triggered visitor gets an empty
  `operator()(const ModifyCounterEffect_t&)`. There is nothing to cross-reference, because the
  id is enum-parsed.

## First counter: `mind_control_total`

In SMAC (Thinker `probe.cpp`):
- Mind Control and Total Thought Control add 4 to the prober's total **when the cost is paid**
  (line 946). That is before the success roll at `MOV_DEFEND`, so a busted attempt still counts.
- Subvert Unit adds 1 **on success** (line 1512, inside the post-roll switch).
- The base mind-control cost adds the **prober's** `total / 4` to its first factor
  (`mod_mind_control`, line 129).

Three things are missing in the repo today:
- Probe actions have no list that fires on payment.
- `ApplySubvertUnit_` never runs `on_success_effects`.
- `QuoteMindControlBaseCost_` has no history term.

**`on_paid_effects`**
1. `ProbeActionConfig_t` gains `onPaidEffects`, parsed from `on_paid_effects` through
   `TriggeredEffectParser::ParseTriggeredEffects`. The parser rejects the key on an action
   without `cost`.
2. `ProbeActionEffects.h` exposes
   `ApplyProbePaidEffects(Unit& rProbe, const ProbeActionConfig_t&, const ProbeTarget_t&, GameState&, std::mt19937&)`.
   It builds the mission context for the target and applies `onPaidEffects`:
   - Base target: `MissionContext_`.
   - Unit target: a new `UnitMissionContext_`, with the actor as the subject faction,
     `pUnit` = the target, `pTile` = its tile, `actionTarget` = its owner, and `pRng`.
3. `ProbeActionExecutor::TryProbeAction` calls it right after `TryPayProbeCost_` succeeds and
   before the roll, passing `m_rRng`.

**Subvert Unit on success**
1. `ApplySubvertUnit_` builds `UnitMissionContext_` and applies `rAction.onSuccessEffects`
   **before** `TransferUnitTo`. That way `actionTarget` and the unit's owner still name the
   victim.
2. Thread `GameState`, `rAction` and the RNG through `ApplyUnitAction_` into
   `ApplySubvertUnit_` (`ApplyProbeActionEffect` already has them).

**Handlers that run `on_success_effects`**
- The probe parser rejects `on_success_effects` on an action whose handler never runs it, as
  the council parser's `ValidatePassedEffectHonored_` does for `on_passed_effects`.
- An exhaustive switch over `ProbeActionId_t` names the handlers that run it:
  `infiltrate`, `sabotage_random`, `genetic_plague` and `subvert_unit`. Every other action
  rejects the key.

**Config**

Author these in both `config/probe_actions.json` and `tests/fixtures/probe_actions.json`:
- `mind_control_base` and `total_thought_control`: `on_paid_effects` holds
  `ModifyCounter mind_control_total 4`, and `cost` gains `"mind_control_divisor": 4`.
- `subvert_unit`: `on_success_effects` holds `ModifyCounter mind_control_total 1`.

**Reader**
1. `QuoteProbeActionCost` takes the acting `const Faction&`. Both callers, `CanProbeAction`
   and `ProbeActionExecutor::TryPayProbeCost_`, pass the probe's faction.
2. `ProbeCostConfig_t` gains `mindControlDivisor`, parsed from `mind_control_divisor`, the
   same way `riot_turns` is handled:
   - It is required and must be positive on a base-target cost.
   - It is rejected on a unit-target cost, because the unit formula never reads it.
   - `ParseCost_` takes the action's target to decide which applies.
3. `QuoteMindControlBaseCost_` takes the actor's total. Its first factor becomes
   `garrison + actorTotal / mindControlDivisor + population`, using integer division, as SMAC
   does.

## Tests

- **Parser: `ModifyCounter`**
  - `ModifyCounter` parses.
  - Rejected: a missing or unknown counter; a missing, zero, negative or fractional amount; a
    `factionFilter`.
  - In a continuous `effects` list it fails with the one-shot message.
- **Parser: probe actions**
  - `on_paid_effects` on an action without `cost` is rejected.
  - `on_success_effects` on an action whose handler never runs it (`steal_tech`) is rejected.
  - `mind_control_divisor` is rejected when missing or zero on a base-target cost, and when
    present on a unit-target cost.
- **Dispatch**
  - It increments the context faction's counter and reports `CounterModified_t`.
  - A multi-faction context credits each faction once.
  - A probe mission context (`pBase` = the target) credits the actor, not the base's owner.
  - A `oncePer` entry fires once.
- **Probe writers**
  - Mind Control adds 4 to the actor's counter; the former owner's is unchanged.
  - Total Thought Control adds 4.
  - A busted Mind Control still adds 4. Force the failed outcome by raising the action's risk
    in the test's config, which zeroes the success rate.
  - Subvert Unit adds 1; a failed Subvert Unit adds nothing.
- **Probe reader**
  - With an actor total of 8 and a divisor of 4, the first factor is
    `garrison + 2 + population`.
  - The target faction's total does not change the quote.
  - The `subvert_unit` quote is unaffected by the actor's total.

Run with `./bd test`.

## Docs

- **`docs/architecture/effects-system.md`**
  - Add `ModifyCounter` to the triggered type list.
  - Add `on_paid_effects` to the `probe_actions.json` row of the trigger-slot table.
  - Add a short Counters section covering: stored facts versus derived stats; the closed enum
    that grows with its readers; faction-scoped storage; and how counters differ from
    `oncePer` keys.
- **`TriggeredEffect.h`:** a comment on `ModifyCounterEffect_t` covering the per-faction
  subject and the positive integer amount.
- **`ProbeActionConfig.h`:** the `ProbeCostConfig_t` formula comment gains the history term;
  comments on `onPaidEffects` and on which handlers run `onSuccessEffects`.
- **`ProbeRules`:** the `QuoteMindControlBaseCost_` formula comment, and the
  `QuoteProbeActionCost` header comment naming the actor.
