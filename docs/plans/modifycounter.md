---
name: Mind control ledger
overview: "A RecordMindControl triggered effect writes SMAC's mind_control_total into a GameState-owned MindControlLedger, read by the base mind-control cost. Paying for Mind Control or Total Thought Control records 4, a successful Subvert Unit records 1, and the acting faction's total raises the base mind-control cost."
todos:
  - id: ledger
    content: MindControlLedger (Record / Total) owned by GameState, sibling of AtrocityLedger
    status: completed
  - id: effect-arm
    content: RecordMindControlEffect_t, ParseRecordMindControl_, ApplyOne_ arm, IsPerFactionSubject_, MindControlRecorded_t, EffectReferenceValidator no-op
    status: completed
  - id: probe-paid-list
    content: on_paid_effects on probe actions (rejected without cost); ApplyProbePaidEffects fired after payment, before the roll
    status: completed
  - id: probe-success-list
    content: Subvert Unit runs on_success_effects; parser rejects on_success_effects on handlers that never run them
    status: completed
  - id: probe-config
    content: Author RecordMindControl entries and mind_control_divisor in config and fixture
    status: completed
  - id: probe-reader
    content: QuoteProbeActionCost takes the actor and the ledger; mind_control_divisor required on base-target costs; actor total / divisor in the first factor
    status: completed
  - id: tests
    content: Parser, dispatch, probe writer and cost tests; ./bd test
    status: completed
  - id: docs
    content: effects-system.md (triggered list, RecordMindControl section, trigger-slot table); TriggeredEffect.h, ProbeActionConfig.h and ProbeRules comments
    status: completed
isProject: false
---

# Mind control ledger

## What it is

SMAC remembers how much mind control each faction has done, and that history raises what the
next base mind-control costs. It is a stored fact, not a derived stat: a `StatId_t` is resolved
from continuous effects and has no storage, so triggers never write one. Stored per-faction
tallies are **one ledger per concern**, like `AtrocityLedger`: this one is `MindControlLedger`.

```json
{ "type": "RecordMindControl", "parameters": { "weight": 4 } }
```

## Design

**`MindControlLedger`** (`include/game/mind-control/MindControlLedger.h`)
- World-scoped, owned by `GameState` beside `AtrocityLedger`, keyed by `FactionId_t`.
- `Record(actor, weight)` rejects a non-positive weight; `Total(actor)` is 0 for a faction with
  nothing recorded.
- A stub holding per-actor totals. Per-act records (victim, kind, year) come with the
  mind-control cost calculator and SMAC's per-target `diplo_mind_control`.
- It is save data, so it serializes with `GameState` once serialization is wired.

**`RecordMindControlEffect_t { int weight; }`**
- **Parser.** `ParseRecordMindControl_` requires `weight` as a positive integer
  (`is_number_integer()`, so 1.5 is rejected rather than truncated). Registered in the type
  table, so the continuous parser rejects it with the one-shot message.
- **Dispatch.** A per-faction subject, like `CommitAtrocity`: it applies once to each faction in
  `context.factions`, never to the context base's owner. In a probe mission the actor is in
  `factions` while `pBase` and `pFaction` name the victim. The `ApplyOne_` arm records the
  weight, pushes `MindControlRecorded_t { weight }` and returns `true`.
- **Validator.** `EffectReferenceValidator`'s triggered visitor has an empty arm: there is
  nothing to cross-reference.

## SMAC's `mind_control_total`

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
  `RecordMindControl` with weight 4, and `cost` gains `"mind_control_divisor": 4`.
- `subvert_unit`: `on_success_effects` holds `RecordMindControl` with weight 1.

**Reader**
1. `QuoteProbeActionCost` takes the acting `const Faction&` and the `MindControlLedger`. Both
   callers, `CanProbeAction` and `ProbeActionExecutor::TryPayProbeCost_`, pass the probe's
   faction; `ProbeActionExecutor` borrows the ledger from `GameState` at construction.
2. `ProbeCostConfig_t` gains `mindControlDivisor`, parsed from `mind_control_divisor`, the
   same way `riot_turns` is handled:
   - It is required and must be positive on a base-target cost.
   - It is rejected on a unit-target cost, because the unit formula never reads it.
   - `ParseCost_` takes the action's target to decide which applies.
3. `QuoteMindControlBaseCost_` takes the actor's total. Its first factor becomes
   `garrison + actorTotal / mindControlDivisor + population`, using integer division, as SMAC
   does.

## Tests

- **Parser: `RecordMindControl`**
  - `RecordMindControl` parses with its weight.
  - Rejected: a missing, zero, negative or fractional weight; a `factionFilter`.
  - In a continuous `effects` list it fails with the one-shot message.
- **Parser: probe actions**
  - `on_paid_effects` on an action without `cost` is rejected.
  - `on_success_effects` on an action whose handler never runs it (`steal_tech`) is rejected.
  - `mind_control_divisor` is rejected when missing or zero on a base-target cost, and when
    present on a unit-target cost.
- **Dispatch**
  - It adds to the context faction's ledger total and reports `MindControlRecorded_t`.
  - A multi-faction context credits each faction once.
  - A probe mission context (`pBase` = the target) credits the actor, not the base's owner.
  - A `oncePer` entry fires once.
- **Probe writers**
  - Mind Control adds 4 to the actor's total; the former owner's is unchanged.
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
  - Add `RecordMindControl` to the triggered type list and the per-faction-subject list.
  - Add `on_paid_effects` to the `probe_actions.json` row of the trigger-slot table.
  - Add a short `RecordMindControl` section naming `MindControlLedger` and its reader.
- **`TriggeredEffect.h`:** a comment on `RecordMindControlEffect_t` covering the per-faction
  subject and the positive integer weight.
- **`ProbeActionConfig.h`:** the `ProbeCostConfig_t` formula comment gains the history term;
  comments on `onPaidEffects` and on which handlers run `onSuccessEffects`.
- **`ProbeRules`:** the `QuoteMindControlBaseCost_` formula comment, and the
  `QuoteProbeActionCost` header comment naming the actor.
