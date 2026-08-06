# Package 5 — Planetary Council: lifecycle, tally, outcomes, config schema

**Source package:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md), Package 5
**Verified against:** working tree at commit `965f464` (after full-review Packages 1–4)

---

## Verified diagnosis

### [H] A pending vote has no exit other than unanimous participation — CONFIRMED

`src/game/council/PlanetaryCouncil.cpp:592` — `Resolve` throws unless `AllMembersVoted()`, and
nothing else clears `m_pending`; `Propose` throws whenever `m_pending` is set (`:276-279`).
So a single member that never casts a ballot bricks the council for the rest of the game.

This is reachable in the shipped build: `CastStubCouncilVotes` is wired only from
`Engine`'s `OnProposalOpened` connection, and it is a free function that reaches back into the
council rather than something the council drives per member. `TallyStandard_` (`:459-462`)
already treats a missing ballot exactly like `Abstain`, so the strict gate buys the tally
nothing — it only creates a terminal state.

### [H] `m_activeProposalIds` conflates "law in force" with "already enacted" — CONFIRMED

`ApplyPassedProposal_` (`:532-536`) calls `ActivateProposal_` for **every** passed
non-election proposal, including ones whose entire content is a repeal.
`config/council/proposals.json` ships two such: `repeal_trade_pact` and `repeal_un_charter`,
both with `"effects": []`.

`CanPropose` rejects `IsActive(id) && !repeatable` (`:252`), so `repeal_un_charter` is consumed
permanently the first time it passes. After `reinstate_un_charter`, the charter can never be
repealed again — the exact flip-flop the code's own comment at `:257-260` says it wants to
support.

**Important:** the config already gates these correctly without the marker.
`repeal_trade_pact` declares `required_proposals: [global_trade_pact]`, and `repeal_un_charter`
declares `requires_rule_flags: [atrocities_forbidden]`. Availability is config-driven; the
marker is pure interference.

### [M] `voteThreshold` is silently ignored for standard ballots — CONFIRMED
`TallyStandard_` (`:452-473`) returns `yea > nay` and never reads `rConfig.voteThreshold`, while
`TallyElection_` (`:509-515`) does honor it. A modder setting `vote_threshold: 0.66` on a
standard proposal gets a plain majority, with no error at load or resolve.

### [M] The "two most populous factions" governor rule is not enforced — CONFIRMED
`CastElectionVote` (`:353`) only checks council membership. `GovernorCandidates()` (`:178-205`)
computes the top two but is consumed only by the AI stub and tests; the UI passes full
membership as the candidate list (`src/ui/council/CouncilVoteView.cpp:73`). The documented rule
lives in a function that enforces nothing.

### [M] Tallying is not separable or testable — CONFIRMED
Both tally helpers are private, take no ballot argument, and dereference `m_pending`. Testing
"does 2 Yea beat 1 weighted Nay" requires a `GameState`, a `WorldMap`, three `Faction`s and
three bases.

### [M] The AI stub is a free function, not a seam the council owns — CONFIRMED
`CastStubCouncilVotes(PlanetaryCouncil&)` re-reads `GetPending()`, re-derives who may vote, and
forces `Members()` to hand out non-const `Faction*` from a const accessor. There is no
interface a real AI could implement.

---

## Chosen design

### A. Absentees abstain; the council drives non-player ballots

`Resolve` drops the `AllMembersVoted` precondition. A member with no ballot is an abstention —
which is what the tally already did. `AllMembersVoted()` stays as a query the UI can use to
decide whether to *offer* resolution, but it is no longer a preconditon for it.

Separately, an `ICouncilVoter` seam lets the council ask each non-player member for a ballot
when a proposal opens, so the shipped build actually produces votes. `CouncilAiStub` becomes
the first implementation instead of a free function reaching in from outside.

*Rejected:* a turn-count timeout on the pending vote. It needs a rule (how many turns?) that
SMAC would have to supply, and it leaves the brick reachable in the meantime.

### B. The in-force set means "carries continuous world effects"

`ActivateProposal_` records a proposal only when `HasContinuousWorldEffects_` holds. Everything
else is history, tracked by `m_passCounts`.

Consumption then needs one adjustment: a proposal that **repeals** something is not a one-shot.
Its availability is already governed by its own config gates (`required_proposals`,
`requires_rule_flags`), which track the target's state — so it is exempt from the
"non-repeatable and already passed" rule. This is not an invented SMAC rule: it is what the
shipped config already expresses and what the existing comment says the code intends.

### C. Honor `voteThreshold` for standard ballots

`TallyStandard_` applies the threshold as a share of *total* vote weight when non-zero, matching
`TallyElection_`. Zero keeps the documented "simple majority" meaning.

### D. `CastElectionVote` validates the candidate

The candidate must be in the eligible set for the proposal's `electionOutcome`:
`GovernorCandidates()` for `PlanetaryGovernor`, all members for `SupremeLeaderVictory`. One
source of truth, enforced where the vote is cast rather than in the UI.

### E. Tally becomes a pure function

Extract `TallyStandardBallots` / `TallyElectionVotes` as free functions over
(members+weights, ballots, threshold) so the voting rules are unit-testable without a session.

---

## Out of scope (deferred, with rationale)

- **`vote_threshold` as `Rational_t`** and the parser-schema items (`kind` vs `election_outcome`
  cross-validation, interval defaults in two places) — these are package 11's config-strictness
  theme; doing them here would duplicate that work.
- **`ComputeVoteWeight` rebuilding the effect pool per member per call** — a caching concern
  that belongs with package 17's memoization work on the same pattern.
- **Election ballot lists every member (`ui/council`)** — ~~package 15~~ **done here after
  review**: enforcing D while the UI still offered every member turned a cosmetic UI bug into
  an exception on selection, so the view now offers exactly `EligibleCandidates()`.

Still open from this package's finding list, deferred with reason:

- **[M] A governor veto on an election can never be overruled.** `VetoUnanimouslyOverruled_`
  reads `m_pending->ballots`, which is always empty for an election (those populate
  `electionVotes`), so a vetoed election is unconditionally `Vetoed`. Fixing it needs a
  *rule*: what does "every non-governor voted Yea" mean for an election — every non-governor
  voted for the same candidate? for any candidate? SMAC has to answer that, so this is a TODO
  rather than a guess (see `.devin/rules/coding-guidelines.md`).
- **[M] The applier ignores per-effect targeting on instantaneous outcomes**
  (`CouncilOutcomeApplier` applies `GrantEnergy` to every member, never consulting
  `factionFilter` / `condition`). Untouched — it is an effects-routing concern, not a lifecycle
  one, and belongs with the effects packages' filter work.
- **[M] A missing member silently turns a won election into a failure** (`ResolveOutcome_`).
  Untouched; interacts with the veto-overrule rule above and should be decided with it.
- **[E] Tally as pure free functions.** Not done. The tallies are still private members reading
  `m_pending`. The behavioural fixes (threshold, eligibility) landed without it, and extracting
  them is a pure refactor better done alongside the veto/absentee rules above so the extracted
  signature is settled once.

## Test plan

- A pending vote resolves when only some members voted (absentees abstain) — the brick is gone.
- `repeal_un_charter` → `reinstate_un_charter` → `repeal_un_charter` again all succeed.
- A pure repeal never appears in the in-force set.
- A standard proposal with `vote_threshold: 0.66` fails on a bare majority and passes above it.
- `CastElectionVote` rejects a non-candidate for a governor election.
- Tally functions unit-tested directly, without a `GameState`.
