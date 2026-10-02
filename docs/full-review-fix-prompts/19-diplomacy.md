# Package 19 — Diplomacy: Vendetta flow, proposals, and trade

Source: the diplomacy review of 2026-10-02. Verified against `d246e71`, the rules/effects split
(`DiplomaticPermissionRules`, `DiplomaticTransitionRules`, `DiplomaticTransitionEffects`,
`TerritoryEntryEffects`). Line numbers below refer to it.

## Scope and split

Two `[H]`, eight `[M]` and twelve `[L]`, landing as **three commits**. Each one builds and passes
`./bd test` on its own.

- **A — Vendetta and defensive obligations.** Entering a Vendetta goes through one core function.
  Obligations are checked when raised and again when answered. Native life is shut out where it
  could enter diplomacy, and the obligation mode is read from config.
- **B — Proposals and trade.** A proposal carries only negotiated changes. Validation and
  application become Rules/Effects free functions built on visitors. The executor keeps only the
  pending slot. The new trade rule lands here.
- **C — Hygiene.** Status display names, ledger storage, and ledger queries only tests use.

Rules settled for this package:

- **Trade:** two factions that have met may trade any item, whatever their status. Bases and
  coordinated Vendetta no longer need a Pact, and a Vendetta no longer blocks trade. It only makes
  the other side less likely to accept, which is a judgement for AI evaluation (still a stub).
- **`TradeDeclareVendetta_t`:** the giver declares, as with every other item the giver provides.
- **Native life:** never met and never holds a diplomatic status.

Constraints: build and test only through `./bd`. Report every test failure, and say whether it
was a requirement change or a bug. Update `docs/architecture/` in the same commit as the code.

## Commit A — Vendetta and defensive obligations

### Findings

| Sev | Finding | Locus |
|---|---|---|
| [H] | A defensive-obligation prompt that no longer applies can still cost the player a Pact | `DiplomaticTransitionEffects.cpp:247-266`, `InteractionPresenter.cpp:343` |
| [M] | Native life joins a universal Vendetta and is met on sight, though it has no diplomacy | `AtrocityEffects.cpp:58-84`, `FirstContactResolver.cpp:62-104` |
| [M] | `DeclareVendetta_` does five things, and its first half copies `JoinVendetta_` | `DiplomaticTransitionEffects.cpp:77-141` |
| [M] | `ResolveDefensiveObligation` and `ResolveTerritoryEntry` take flag arguments | `DiplomaticTransitionEffects.h:51`, `TerritoryEntryEffects.h:13` |
| [M] | The obligation mode is hard-coded, and `HonorDefensiveObligation` is public only for tests | `DiplomaticTransitionEffects.cpp:31-32` |
| [L] | `ApplyStatusChange` writes the ledger, then returns silently when a faction is missing | `DiplomaticTransitionEffects.cpp:165-170` |
| [L] | `ExpireDiplomaticStatuses` dereferences `*StepDown(status)` unchecked | `DiplomaticTransitionEffects.cpp:200` |
| [L] | Four self-pair guards repeat the throw `FactionPair::Canonical` already makes | `DiplomaticTransitionEffects.cpp:80`, `:132`, `:148`, `:231` |
| [L] | `IsNativeLifeFaction` lives in `BaseConquestRules.h`, though most of its callers have nothing to do with conquest | `BaseConquestRules.h:27` |
| [L] | `VendettaEntry_t entry` collides with "territory entry" in the same subsystem | `DiplomaticStatus.h:27` |

### Diagnoses

**The prompt that no longer applies.** `DeclareVendetta_` skips a partner already at Vendetta with
the aggressor (`:105`). A queued `PactObligationInteraction_t` is answered later, and nothing checks
that again. `PlayerActions` runs an AI's whole pass without yielding, so this sequence is possible:

1. C attacks the player's Pact partner B, which queues the prompt.
2. C attacks the player, so the player is now at Vendetta with C.
3. The prompt is shown. "Stand aside" steps the Pact with B down, although the player is already
   at war with C.

**Native life.** The rule is checked at four sites in `DiplomaticTransitionEffects.cpp` (`:57`,
`:94`, `:103`, `:237`). It is not checked on the two paths where native life actually gets in:

- `ApplyUniversalVendetta_` skips humans but not native life, which is an AI faction
  (`Engine.cpp:285-286`). After a Major atrocity, native life holds a Vendetta and a commlink with
  the perpetrator. This is reachable today.
- `FirstContactResolver` meets native life on sight. So it is listed in the commlinks panel, and
  the proposal path, which keys on contact, would accept a Treaty with it.

### Changes

**A1. Rename** `VendettaEntry_t` → `VendettaKind_t`, and every `entry` parameter or field → `kind`.
This includes `PactObligationInteraction_t::entry`.

**A2. Move `IsNativeLifeFaction`** to `FactionConfig.h`, inline beside `FactionSpecies_t`.
- Its callers are `Engine.cpp`, `BaseConquestEffects.cpp`, `PlanetPearls.cpp`, `FungalBloom.cpp`
  and `StackCollateral.cpp`.
- Include `FactionConfig.h` where it is called.
- All of them except `BaseConquestEffects.cpp` drop `BaseConquestRules.h`, which they included
  only for this function.

**A3. Add `GameState::RequireFaction(FactionId_t)`**, const and non-const. It returns the faction,
or throws `std::invalid_argument` naming the id. A6, B3 and B4 use it.

**A4. `DiplomaticPermissionRules`** gains:

```cpp
// False for native life: it is never met and never holds a diplomatic status.
bool HasDiplomacy(const Faction& rFaction);

// partner's status with ally carries defensive_obligation, and partner is not already at
// Vendetta with aggressor. False when partner is ally or aggressor.
bool IsObligedToDefend(const GameState& rGameState, FactionId_t partner, FactionId_t ally,
                       FactionId_t aggressor);
```

**A5. Config.**
- `DiplomacyConfig_t` gains `DefensiveObligationMode_t defensiveObligationMode`.
- `DiplomacyConfigParser` requires a top-level `"defensive_obligation_mode"` string and adds it to
  the known top-level keys. It reads the value with `EnumFromName`, using "defensive obligation
  mode" as the field name.
- The parser refuses `"defensive_obligation": true` on a status with no `StepDown` (Neutral,
  Vendetta). Declining an obligation must have a rung to step down to.
- `config/diplomacy.json` and `tests/fixtures/diplomacy.json` gain
  `"defensive_obligation_mode": "JoinAsDefender"`.
- Delete `k_DefensiveObligationMode`.

**A6. `DiplomaticTransitionEffects`.** The public surface becomes:

```cpp
void ApplyStatusChange(GameState&, FactionId_t a, FactionId_t b, DiplomaticStatus_t to);
void ExpireDiplomaticStatuses(GameState&);
void DeclareVendetta(GameState&, FactionId_t declarer, FactionId_t target);
void JoinVendetta(GameState&, FactionId_t defender, FactionId_t aggressor);
void ApplyHostileAct(GameState&, FactionId_t aggressor, FactionId_t victim);
void HonorDefensiveObligation(GameState&, FactionId_t partner, FactionId_t aggressor,
                              VendettaKind_t kind);
void DeclineDefensiveObligation(GameState&, FactionId_t partner, FactionId_t ally);
```

Private functions in the anonymous namespace:

- **`bool EnterVendetta_(GameState&, FactionId_t a, FactionId_t b, VendettaKind_t kind)`**
  - Returns false when the pair is already at Vendetta. Check this first, so a self-pair throws
    from `FactionPair`.
  - Returns false when either side fails `HasDiplomacy`.
  - Otherwise it calls `ApplyStatusChange` to Vendetta and, for a `Declaration`, calls
    `WithdrawFromTerritories_`.
- **`void RaiseDefensiveObligations_(GameState&, FactionId_t aggressor, FactionId_t ally, VendettaKind_t kind)`**
  - For each faction, ask `IsObligedToDefend(partner, ally, aggressor)` when the loop reaches it.
    There is no snapshot, so later partners see earlier partners' answers.
  - An obliged player is queued a `PactObligationInteraction_t`, unless one for (ally, aggressor)
    is already queued.
  - An obliged AI asks `AiHonorsDefensiveObligation_`, then calls `HonorDefensiveObligation` or
    `DeclineDefensiveObligation`.
- **`void DeclareVendetta_(GameState&, FactionId_t declarer, FactionId_t target, VendettaKind_t kind)`**
  - `if (EnterVendetta_(…)) RaiseDefensiveObligations_(rGameState, declarer, target, kind);`
- Keep `WithdrawFromTerritories_`, `IsObligationQueued_` and `AiHonorsDefensiveObligation_`.
- Delete `HasDiplomacy_`, `WithdrawOnDeclaration_`, `JoinVendetta_`, `ResolveDefensiveObligation`
  and the four self-pair throws.

Public function bodies:

- `ApplyStatusChange`: call `RequireFaction` for both sides before touching the ledger. The rest is
  unchanged.
- `ExpireDiplomaticStatuses`: use `StepDown(status).value()`.
- `DeclareVendetta` calls `DeclareVendetta_(…, Declaration)`. `JoinVendetta` calls
  `EnterVendetta_(…, Declaration)`.
- `ApplyHostileAct`: `if (!StatusRulesFor(…).bMayAttack) DeclareVendetta_(…, SneakAttack);`.
  `EnterVendetta_` already handles native life and missing factions.
- `HonorDefensiveObligation` switches on the config's mode, passing `kind` either way:
  - `JoinAsDefender` → `EnterVendetta_`
  - `SeparateDeclaration` → `DeclareVendetta_`
- `DeclineDefensiveObligation` throws `std::logic_error` when partner's status with ally carries no
  obligation. Otherwise it calls `ApplyStatusChange(partner, ally, StepDown(status).value())`.

The header drops its `DiplomacyConfig.h` include, and the source drops `BaseConquestRules.h`.
The header comments say native life is a no-op, not that "its status changes".

**A7. `FirstContactResolver` never meets a faction without diplomacy.** `MeetIfNeeded_` takes the
two factions, and both scans skip a pair where either side fails `HasDiplomacy`.
- Native life drops out of the commlinks panel.
- Every path that keys on contact then excludes native life with no check of its own: proposals,
  the action menu, `TradeCommFrequency_t` and `TradeDeclareVendetta_t`.
- `AtrocityEffects.cpp` needs no change, because `JoinVendetta` refuses native life.

**A8. `TerritoryEntryEffects`.** `ResolveTerritoryEntry(…, bool)` becomes
`BreakAgreementAndContinue(GameState&, Unit&, FactionId_t territoryOwner)`, which declares
Vendetta on the owner and resumes the order. Cancelling is `Unit::ClearOrder()` at the call site.

**A9. `InteractionPresenter`.**
- `PresentPactObligation_`:
  - After the existing null checks, if `!IsObligedToDefend(player, ally, aggressor)`, call
    `CompleteAndAdvance_()` and return.
  - "Declare Vendetta" calls `HonorDefensiveObligation`.
  - "Stand aside" calls `DeclineDefensiveObligation`.
  - The label uses `StepDown(status).value()`.
- `PresentTerritoryEntry_`:
  - "Break" calls `BreakAgreementAndContinue`.
  - "Cancel the order" calls `pResolve->ClearOrder()`.

### Tests

New:
- `DiplomaticPermissionRulesTests`, `IsObligedToDefend`:
  - true for the ally's Pact partner;
  - false once that partner is at Vendetta with the aggressor;
  - false under a status without the obligation;
  - false for the ally and the aggressor themselves.
- `DiplomaticTransitionEffectsTests`:
  - Declining when there is no obligation throws.
  - "Native life has no diplomacy to change" also covers `DeclareVendetta` and `JoinVendetta`.
  - Changing a status with a faction id not in the session throws, and leaves the ledger
    untouched.
- `InteractionPresenterTests`: add two AI factions to the harness session, and set statuses on the
  ledger.
  - An obligation the player no longer owes (already at Vendetta with the aggressor) completes
    without a modal, advances, and leaves the Pact intact.
  - An obligation the player still owes presents a modal and stays queued.
- `AtrocityTests`: native life does not join a universal Vendetta. It ends with no status and no
  commlink with the perpetrator (use `AddNativeLifeFaction`).
- `FirstContactTests`: seeing native life establishes no contact, in either direction.
- `DiplomacyConfigParserTests`:
  - `defensive_obligation_mode` is required;
  - an unknown mode is rejected;
  - `SeparateDeclaration` parses;
  - `defensive_obligation: true` on Neutral or Vendetta is rejected.

Updated:
- Every `ResolveDefensiveObligation(…, true/false, …)` call becomes Honor or Decline, and `entry`
  becomes `kind`.
- "Honoring an obligation as a separate declaration obliges the aggressor's partners" sets
  `fixtures.dataContext.diplomacyConfig->defensiveObligationMode` to `SeparateDeclaration` instead
  of passing a mode. The `JoinAsDefender` case uses the fixture's mode.
- `TerritoryEntryTests`:
  - The "break" cases call `BreakAgreementAndContinue`.
  - Delete "Cancelling at the border drops the order and keeps the Treaty". Cancel is now the
    presenter's `ClearOrder()` call, with no diplomacy function behind it.

### Docs

- `diplomacy-system.md`:
  - The diagram: `EnterVendetta_` / `DeclareVendetta_` / `RaiseDefensiveObligations_`, Honor and
    Decline, `IsObligedToDefend`, `HasDiplomacy`, first contact, `BreakAgreementAndContinue`.
  - Native life is never met and never holds a status.
  - `defensive_obligation_mode`, and the parser's new refusal.
  - "Declaration vs. sneak attack" uses `VendettaKind_t`.
  - Obligations are checked again when answered.
  - The `defensive_obligation` rows of "Where each rule is read" and "Where the rules live".
- `player-interaction-system.md`: the two table rows.
- `atrocity-system.md`: a universal Vendetta never includes native life.
- `high-level.md` and `unit-movement-system.md`: `ResolveTerritoryEntry` →
  `BreakAgreementAndContinue`.

## Commit B — Proposals and trade

### Findings

| Sev | Finding | Locus |
|---|---|---|
| [H] | Declaring Vendetta and cancelling go through accept/reject, so a player recipient can veto them | `DiplomaticActionExecutor.cpp:88-99`, `:304-320` |
| [M] | A tech offered twice passes validation, then throws partway through applying | `DiplomaticActionExecutor.cpp:193-222`, `ResearchManager.cpp:207-210` |
| [M] | `TradeDeclareVendetta_t` is carried out by the receiver, unlike every other item | `DiplomaticActionExecutor.cpp:283-291`, `:370-373` |
| [M] | The executor holds the pending slot, validates and applies | `DiplomaticActionExecutor.h`, `.cpp` |
| [M] | `ValidateItem_` / `ApplyItem_` are `if constexpr` chains, so a new alternative compiles and is silently rejected or ignored | `DiplomaticActionExecutor.cpp:239-295`, `:333-376` |
| [L] | An empty proposal is valid, and takes the player's only slot | `DiplomaticActionExecutor.cpp:131-191` |
| [L] | `FindFaction_` / `FindOwnedBase_` copy `GameState::FindFaction` / `Faction::FindBase`, and one overload of each is never called | `DiplomaticActionExecutor.cpp:24-70` |
| [L] | `KindOf` is unused; it and `ToString(TradeKind_t)` live away from their enum | `DiplomacyActions.cpp:138-164` |
| [L] | `RequireKnown_` only wraps `AreKnown`; `ToString(DiplomaticActionKind_t)` returns "Unknown" | `DiplomacyActions.cpp:17-20`, `:135` |
| [L] | Comments recount old bugs; the executor tests build their own fixture; an economy test sits in the diplomacy file | `DiplomaticActionExecutor.h:37-38`, `.cpp:90-92`, tests |

### Changes

**B1. Take one-sided acts out of proposals.**
- `DiplomaticProposal_t::requestedStatus` is either nullopt or `StepUp` of the current status.
- Add `CancelTreaty(GameState&, FactionId_t a, FactionId_t b)` to `DiplomaticTransitionEffects`.
  It throws `std::logic_error` when their status has no `StepDown`, and otherwise calls
  `ApplyStatusChange` to that status.
- Declaring is done with `DeclareVendetta`.
- Delete `CanRequestStatus`.
- The menu actions map to calls as follows:
  - `CancelTreaty` → `CancelTreaty`
  - `DeclareVendetta` → `DeclareVendetta`
  - `Propose*` and `Trade` → `Propose`

**B2. Add `DiplomaticProposalRules`**, in a new `include/game/faction/DiplomaticProposalRules.h`
and `src/game/faction/DiplomaticProposalRules.cpp`:

```cpp
bool IsValidProposal(const GameState& rGameState, const DiplomaticProposal_t& rProposal);
```

`IsValidProposal` holds when all of these do:
1. The proposer and recipient are distinct session factions (`FindFaction`) that have met.
   Having met is the only relationship gate: items may be traded under any status.
2. The proposal asks for something: a requested status, or at least one item.
3. Any requested status is `StepUp` of the current status, and `CanProposeStepUp` holds.
4. Each side passes `IsValidSide_(rGameState, rGiver, rReceiver, rItems)`. Give runs from proposer
   to recipient; demand runs from recipient to proposer.

`IsValidSide_` holds when every item passes `CanDeliver_`, no base id or tech id appears twice, and
the side's total credits pass `rGiver.GetEconomy().CanAfford`.

`CanDeliver_` is a visitor struct that holds `const GameState&`, `const Faction& rGiver` and
`const Faction& rReceiver`. It has one `bool operator()` per alternative:

| Item | Valid when |
|---|---|
| `TradeCredits_t` | `amount > 0` and `CanAfford(amount)` |
| `TradeTechnology_t` | the giver has it and the receiver does not |
| `TradeBase_t` | `rGiver.FindBase(baseId)` |
| `TradeCommFrequency_t` | it names a third session faction the giver has met and the receiver has not |
| `TradeWorldMap_t` | the giver's explored map is sized |
| `TradeDeclareVendetta_t` | it names a third session faction the giver has met and is not at Vendetta with |

**B3. Add `DiplomaticProposalEffects`**, as a new header and source:

```cpp
// Applies a proposal IsValidProposal accepts: the requested status, then give, then demand.
// Not transactional (see diplomacy-system.md).
void ApplyProposal(GameState& rGameState, const DiplomaticProposal_t& rProposal);
```

- `Deliver_` is a visitor that holds `GameState&`, `Faction& rGiver` and `Faction& rReceiver`, with
  one `void operator()` per alternative.
- The bodies are today's `ApplyItem_` branches, except that `TradeDeclareVendetta_t` calls
  `DeclareVendetta(rGameState, rGiver.GetFactionId(), againstFactionId)`.
- Get the factions with `RequireFaction`.

**B4. `DiplomaticActionExecutor`** keeps `Propose`, `Accept`, `Reject`, `GetPendingProposal`,
`m_pending` and `EvaluateResponse_`, which now takes `const GameState&`.
- `EvaluateResponse_`'s "always agree" comment becomes a TODO. Until an AI attitude model exists,
  every AI accepts. When it exists, the relationship should weigh on the answer, and a recipient at
  Vendetta with the proposer is less likely to accept.
- `Propose` checks in order:
  1. `IsValidProposal` fails → `Invalid`.
  2. The recipient (`RequireFaction`) is the player → `Busy` or `PendingPlayer`.
  3. `EvaluateResponse_` refuses → `Rejected`.
  4. Otherwise `ApplyProposal` → `Accepted`.
- `Accept` validates again with `IsValidProposal`, then applies.
- Delete everything else, including `GiverCost_t` and the comment in `Propose`.

**B5. `DiplomacyActions`** keeps only the menu: `GetAvailableActions` and
`ToString(DiplomaticActionKind_t)`.
- Delete `CanTrade` and `RequireKnown_`. With trade open under every status, `CanTrade` would only
  repeat `AreKnown`.
- Delete `GetAvailableTrades`. The trade kinds no longer depend on the pair, so B6's
  `TradeKinds()` lists them.
- `GetAvailableActions` offers `Trade` whenever the pair has met, Vendetta included.
- `ToString(DiplomaticActionKind_t)` throws on an unhandled value, as `ToString(TradeKind_t)` does.

**B6. `TradeItem`** gains `std::vector<TradeKind_t> TradeKinds()`.
- It returns one kind per `TradeItem_t` alternative, in variant order, built through
  `TradeKindOf` with an index-sequence pack expansion.
- An alternative without a trait still fails to compile, and no default-constructed probe items
  are needed.
- Move `ToString(TradeKind_t)` here too, and delete `KindOf`.
- The `TradeKind_t` comment no longer says "relationship-gated".

### Tests

`DiplomaticActionExecutorTests` moves onto `DiplomacyFixture`, and `DiplomacyGame_` is deleted.
- Add `MeetAll()` to the fixture, setting contact among A, B and C.
- A and B already own a base in that fixture. Base tests find the transferred base with `FindBase`
  and count bases relative to the starting ones.

Requirement changes (record them in the commit message):
- "A proposal must request the next status up, down, or Vendetta": now only the next status up is
  accepted. A step down or Vendetta is `Invalid`.
- Delete "Base transfer without Pact is invalid". "Base transfer changes ownership" drops its Pact
  setup.
- "Third-party vendetta trade sets status": the giver is now at Vendetta with the target, for give
  and for demand.
- "Trade under vendetta is invalid" becomes "A Truce can be bought with credits". At Vendetta, a
  proposal requesting Truce and giving credits is accepted: the status becomes Truce and the
  credits move.
- `DiplomacyActionsTests`:
  - "Vendetta blocks trade and allows only truce": Vendetta now offers `ProposeTruce` and `Trade`.
  - Delete "Bases and coordinated vendetta require Pact", and every `CanTrade` and
    `GetAvailableTrades` check.
- "Declaring Vendetta by proposal obliges the target's Pact partners" now calls `DeclareVendetta`.
  Delete `DiplomacyFixture::ProposeVendetta`; its callers call `DeclareVendetta` directly.
- `DiplomaticTransitionRulesTests`: delete the `CanRequestStatus` case along with the function.

New:
- An empty proposal is invalid.
- The same tech offered twice is invalid, and nothing applies.
- `CancelTreaty` steps Pact → Treaty and Treaty → Neutral, and throws under Neutral and Vendetta.
- `TradeKinds()` lists each `TradeKind_t` exactly once.

Moved and deleted:
- Move "EconomyManager owns the never-negative rule" to `EconomyManagerTests.cpp`.
- Delete "DiplomaticActionKind_t and TradeKind_t ToString are non-empty".
- Delete the opening comments of the aggregate-cost test and the `Busy` test, which recount the old
  bugs.

### Docs

- `diplomacy-system.md`:
  - The diagram: the proposal rules, effects and executor, and `CancelTreaty`.
  - "Transitions": proposals only step up; cancelling and declaring are one-sided.
  - The list of declaration sources.
  - The `TradeItem_t` / `TradeKind_t` section: factions that have met may trade any item under any
    status, and `TradeKinds()` goes through the trait. Drop the paragraph on how the mapping used
    to be kept.
  - "Not yet built": AI evaluation should weigh the relationship, and a Vendetta makes acceptance
    less likely.
  - The proposal lifecycle and validation sections: `IsValidProposal`, `IsValidSide_`, empty
    proposals, and duplicate items. Drop the sentence recounting the old negative-treasury bug.
  - "Where the rules live".
- `faction-system.md`: the executor's edges to `Faction` become `ApplyProposal`'s.

## Commit C — Hygiene

**C1.** `ToString(DiplomaticStatus_t)` returns the status name via magic_enum for every status,
Neutral included. `CommlinksPanel` draws the status only when it is not Neutral. Prompts and
`CommerceDisplay` then never print an empty status. Severity: [L].

**C2.** `DiplomacyLedger`: `m_known` becomes `std::set<FactionPair>` and `m_infiltration` becomes
`std::set<DirectedFactionPair>`. Delete `HasTruce`, `HasTreaty` and `HasPact`. Only tests use them
once commit B deletes `CanTrade`, the one production caller of `HasPact`. Tests compare `GetStatus`
instead. Severity: [L].

## Acceptance

- `./bd test` passes after each commit.
- Outside the historical review docs, nothing references `ResolveDefensiveObligation`,
  `ResolveTerritoryEntry`, `CanRequestStatus`, `VendettaEntry_t`, `k_DefensiveObligationMode`,
  `HasDiplomacy_`, `WithdrawOnDeclaration_`, `JoinVendetta_`, `FindOwnedBase_`, `GiverCost_t`,
  `ValidateItem_`, `ValidateGiverTotals_`, `ApplyItem_`, `RequireKnown_`, `CanTrade`,
  `GetAvailableTrades`, `KindOf(const TradeItem_t`, `HasTruce`, `HasTreaty` or `HasPact`.
- Each behaviour test fails when its fix is reverted. These are the stale prompt, native life, the
  duplicate tech, the empty proposal, proposals that try to cancel or declare, and trade under
  Vendetta.
