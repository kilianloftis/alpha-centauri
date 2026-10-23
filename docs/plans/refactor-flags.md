# Refactor flags

Rule violations found during the [readable-code cleanup](readable-code-cleanup.md) that need
more than a local fix. Each entry looks like this:

```markdown
### `Name` — what is wrong
- **Where:** file and line; callers
- **Problem:** what the code does that breaks the rule.
- **Waits on:** the feature or change that resolves it.
```

## Code waiting on a feature

Rule 6. These have no production caller because the feature that would call them is not built.
They stay until that feature lands, and this list records what is left to build.

### `AIProfile` — tendencies nothing reads
- **Where:** `include/game/faction/AIProfile.h`; `Faction::m_pAIProfile`
- **Problem:** the class only holds the parsed `ai.json` tendencies for an AI that does not
  exist.
- **Waits on:** the AI.

### `EventBus::Unsubscribe` and `ScopedConnection::IsConnected` — used only by tests
- **Where:** `include/lib/EventBus.h:29`, `include/lib/Signal.h:73`; `tests/lib/EventBusTests.cpp`,
  `tests/lib/SignalTests.cpp`
- **Problem:** no production caller; the tests check unsubscribe semantics for mod handlers that
  do not exist yet.
- **Waits on:** mod event handlers.

### Faction save/load and flavour paths used only by tests
- **Where:** `Faction::ExtractBase` (7 test callers), `Faction::CreateBaseFromSnapshot` (2),
  `BaseManager::CaptureSnapshot` and `BaseSnapshot_t`, `FactionFlavor::PickPhrase` (2),
  `ResearchSelector::SetCategoryEnabled` (20)
- **Problem:** no production caller.
- **Waits on:** save/load (the snapshot pair), diplomacy dialogue (`PickPhrase`) and the research
  priority UI (`SetCategoryEnabled`).

### Production and economy entry points used only by tests
- **Where:** `ComputeCommerceForFaction` and `ComputeCommerceForBase` in `CommerceRules.h` (17
  test callers each), `EconomyManager::SetEnergyAllocation` (7), `ResourceManager::GetPsych` (5),
  `WorkerAssignmentManager::SetTileScorer` (1), `ProductionManager::Replace` and `Delete` (2)
- **Problem:** no production caller. Production reaches commerce through `CommerceCache` and
  `ComputeCommerceLines`.
- **Waits on:** the energy sliders (`SetEnergyAllocation`), build-queue editing (`Replace`,
  `Delete`) and the worker governor (`SetTileScorer`). The commerce functions and `GetPsych`
  have no feature of their own, so their tests can move to `ComputeCommerceLines` and the
  getter production reads whenever these are next touched.

### Diplomacy, council and atrocity queries used only by tests
- **Where:** the whole `DiplomaticActionExecutor` (`Propose`, `Accept`, `Reject`,
  `GetPendingProposal`) and `GameState::GetDiplomaticActionExecutor` (5); `AtrocityLedger::HasVictimized`
  (6), `HasCommittedMajorAgainst` (5), `SanctionUntilYear` (4); `PenaltiesApplied` in
  `AtrocityMutations.h` (18); `CouncilVoteExecutor::VetoPending` (3), `AllMembersVoted` (3);
  `PlanetaryCouncil::GetSupremeLeaderVictoryStub` (2); `GetAvailableActions` in
  `DiplomacyActions.h` (5); `DiplomacyLedger::AddGrievance` (1), `AddIntegrity` (1);
  `MayRepairAt` (4); `TradeKinds` (1)
- **Problem:** no production caller. Nothing proposes a diplomatic action, so the executor is
  unwired end to end.
- **Waits on:** the diplomacy UI and AI (the executor, the ledger queries, `GetAvailableActions`,
  `TradeKinds`), the council UI (`VetoPending`, `AllMembersVoted`), the diplomatic victory
  check (`GetSupremeLeaderVictoryStub`) and per-turn healing (`MayRepairAt`).
