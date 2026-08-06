## Planetary Council — runtime and outcomes

**Files:** `src/game/council/PlanetaryCouncil.cpp`, `include/game/council/PlanetaryCouncil.h`,
`src/game/council/CouncilOutcomeApplier.cpp`, `include/game/council/CouncilOutcomeApplier.h`,
`src/game/council/CouncilEffects.cpp`, `include/game/council/CouncilEffects.h`,
`src/game/council/CouncilAiStub.cpp`, `include/game/council/CouncilAiStub.h`

**Assessment:** The split into vote lifecycle / continuous-effect store / outward mutation is
genuinely good, the constructor enforces its membership invariants, and `Resolve` has a single
exit that clears `m_pending` before emitting `OnResolved` — the ordering hazards are handled.
The dominant weakness is the state model itself: the "state machine" is one
`std::optional<PendingProposal_t>` plus a `std::vector<std::string>` of active ids that means
two different things, with no way to leave a pending vote once entered. Vote tallying and the
election-candidate rule are also not where they claim to be.

### [H] `m_activeProposalIds` conflates "law in force" with "already enacted"
`src/game/council/PlanetaryCouncil.cpp:532-535` — every passed non-election proposal is pushed
into the active set, even one whose only content is a repeal or an instantaneous grant. Since
`CanPropose` rejects `IsActive(id) && !repeatable` (`:252`), a pure repeal is consumed for the
rest of the game: `repeal_un_charter` (`config/council/proposals.json:105`, no effects, not
repeatable) becomes permanently "active", so after `reinstate_un_charter` the charter can never
be repealed again — the exact flip-flop the code's own comment at `:257-260` says it wants to
support. The same conflation lets `requiredProposals` gates be satisfied by one-shots that are
not laws. Fix direction: keep the in-force set to proposals that actually contribute continuous
effects (`HasContinuousWorldEffects_` already exists), and drive repeatability from
`m_passCounts` plus a config field rather than from the marker.

### [H] A pending vote has no exit other than unanimous participation
`src/game/council/PlanetaryCouncil.cpp:592` — `Resolve` throws unless `AllMembersVoted()`, and
nothing can clear `m_pending` (no cancel, no timeout, no "absent counts as abstain" resolve),
while `Propose` throws whenever `m_pending` is set (`:276-279`). Any member that never casts a
ballot therefore bricks the council permanently. That is reachable in the shipped build today:
`CastStubCouncilVotes` has no caller outside `tests/game/PlanetaryCouncilTests.cpp` (verified by
search; `Engine.cpp` only creates the two views), so the AI members of a player-initiated vote
never vote, `CouncilVoteView::TryResolveAndClose_` silently returns, Escape closes the view with
`m_pending` still set, and `CommlinksView::OnProposalSelected_` then no-ops forever. Note
`TallyStandard_` already treats a missing ballot exactly like `Abstain` (`:459-462`), so the
strict gate buys nothing the tally needs. Fix direction: make participation a council concern —
resolve absentees as abstentions and/or drive ballots through an injected voter abstraction (see
below) — rather than leaving the terminal state reachable.

### [M] The AI stub is a free function, not a seam the council owns
`src/game/council/CouncilAiStub.cpp:10` — `CastStubCouncilVotes(PlanetaryCouncil&)` is a global
that reaches into the council from outside: it re-reads `GetPending()`, re-derives who may vote,
and needs `Members()` to hand out non-const `Faction*` from a const accessor
(`include/game/council/PlanetaryCouncil.h:58`). There is no `ICouncilVoter`-style interface, so a
real AI cannot be substituted or tested against the council — it can only be a second free
function with the same reach-in, and every caller (currently only tests) must know to invoke it.
It also hardcodes rules the council owns: `GovernorCandidates()` is used as the candidate pool
for *any* election including Supreme Leader (`:33`), and all AI members are given one shared
choice computed once (`:20-39`). Fix direction: define a narrow voter interface (`Decide(pending,
member) -> ballot`) that the council calls for non-player members when a proposal opens, and make
the stub the first implementation.

### [M] The "two most populous factions" governor rule is not enforced
`src/game/council/PlanetaryCouncil.cpp:353` — `CastElectionVote` only checks that the candidate is
a council member, so any member can be elected Planetary Governor. `GovernorCandidates()`
(`:178-205`) computes the top two but is consumed only by the AI stub and tests; the UI passes the
full membership as the candidate list (`src/ui/council/CouncilVoteView.cpp:73`). The rule stated in
the proposal description and in `docs/architecture/council-system.md` therefore lives in a function
nobody enforcing anything calls — two sources of truth, and the UI is the one that decides.
`CastElectionVote` should validate the candidate against the eligible set for the proposal's
`electionOutcome`.

### [M] `voteThreshold` is silently ignored for standard ballots
`src/game/council/PlanetaryCouncil.cpp:452-473` — `TallyStandard_` returns `yea > nay` and never
reads `rConfig.voteThreshold`, though the field is documented as "fraction of total vote weight
required for passage (elections / specials); 0 = simple majority"
(`include/game/council/CouncilProposalConfig.h:39-41`). A modder who sets `vote_threshold: 0.66`
on a standard proposal gets a plain majority with no error at load or at resolve. Either honor the
threshold in `TallyStandard_` or reject a non-zero threshold on `Standard` proposals.

### [M] A governor veto on an election can never be overruled
`src/game/council/PlanetaryCouncil.cpp:391` — `VetoUnanimouslyOverruled_` looks only at
`m_pending->ballots`, which is always empty for `Election` proposals (they populate
`electionVotes`). So the documented escape hatch ("every non-governor voted Yea") is structurally
unreachable for elections, giving the governor an absolute, unappealable veto over both governor
re-election and the Supreme Leader victory. If that asymmetry is intended it must be stated and
tested; otherwise the overrule test needs an election form (every non-governor voted for the same
candidate).

### [M] Tallying is not separable or testable
`src/game/council/PlanetaryCouncil.cpp:452`, `:476` — both tally helpers are private, take no
ballot argument, and dereference `m_pending` on the assumption that only `ResolveOutcome_` calls
them; the invariant is implied, not asserted. Testing "does 2 Yea beat 1 weighted Nay" currently
requires a `GameState`, a `WorldMap`, three `Faction`s and three bases (see the `CouncilGame_`
fixture). Extracting the tally as a free function over (members, weights, ballots, threshold)
would make the voting rules unit-testable and let `Resolve` stay a thin driver — which is what the
architecture doc already claims the split achieves.

### [M] `ActiveEffect_t::config` pointers into a rebuilt vector outlive their guarantee
`include/game/council/CouncilEffects.h:21-25` claims the wrappers are kept with their backing
configs "so the non-owning `ActiveEffect_t::config` pointers stay valid across rebuilds", but
`RebuildWorld` clears and refills `m_worldConfigs` (`src/game/council/CouncilEffects.cpp:20-36`),
so any wrapper handed out earlier by `CollectWorldEffects()` — which returns a *copy* of the
vector (`PlanetaryCouncil.cpp:606`) — points into reassigned storage after the next rebuild. This
is latent rather than live today (I checked the consumers: `GameState::CollectWorldEffects` and the
stages use the result within the call, and no cache retains it), but it breaks the documented
contract that `config` "points into static config data"
(`include/game/effects/ActiveEffect.h:32`), and `ApplyPassedProposal_` rebuilds once per repeal
plus once on activation. Either state the real constraint ("valid until the next rebuild; never
retain") or give the configs stable storage (deque / `unique_ptr` nodes).

### [M] The applier ignores per-effect targeting on instantaneous outcomes
`src/game/council/CouncilOutcomeApplier.cpp:23-35` — `GrantEnergy` is applied to every council
member unconditionally; `rEffect.factionFilter` and `rEffect.condition` are never consulted, so a
proposal that grants energy only to the proposer or only to signatories silently pays everyone.
Any other Instantaneous variant falls through both branches with no diagnostic. The scope/variant
inertness may be the project's deliberate "legal but inert" policy, but silently *widening* a
declared `factionFilter` is a different failure — it produces wrong values rather than none.

### [M] `ComputeVoteWeight` rebuilds the whole effect pool per member, per call
`src/game/council/PlanetaryCouncil.cpp:163-171` — each call copies the faction's entire cached
effect pool, then appends copies of the governor and world effect vectors, to read a single
`CouncilVotes` stat. `TallyStandard_`/`TallyElection_` call it once per member, and
`CouncilFactionVotesPanel::Render` calls it once per member *per frame*
(`src/ui/council/CouncilFactionVotesPanel.cpp:119`). `CollectWorldEffects()` returning by value
when `CouncilEffects::WorldEffects()` already exposes a const reference is the avoidable half of
this; filtering by `StatId_t::CouncilVotes` before copying is the other.

### [M] A missing member silently turns a won election into a failure
`src/game/council/PlanetaryCouncil.cpp:517` — `FindMember_(bestId)` can return null, and
`ResolveOutcome_` infers `passed = pElectionWinner != nullptr` (`:572`), so an unresolvable winner
id is reported as `Failed` instead of as the invariant violation it is. Per the project guideline
("throw errors if expected pointers are null") this should throw; as written the only signal is a
wrong outcome.

### [L] Convention and hygiene items
- `src/game/council/PlanetaryCouncil.cpp:367-376` — `VetoPending` returns `false` on precondition
  failure while every other entry point in the class throws; inconsistent, and the UI never checks.
- `src/game/council/PlanetaryCouncil.cpp:559-563` — `VetoUnanimouslyOverruled_()` is evaluated
  twice to compute two complementary booleans; one call into a local reads better.
- `src/game/council/PlanetaryCouncil.cpp:532-545` — activation is decided by a negated
  two-enumerator condition and then repeated positively in the governor branch; a single `switch`
  on `electionOutcome` would state the three cases once.
- `src/game/council/PlanetaryCouncil.cpp:200` — "top two" governor candidates is a hardcoded `2`;
  it belongs in `CouncilRulesConfig_t` with the propose intervals.
- `src/game/council/PlanetaryCouncil.cpp:180` — local data struct `Entry` should be `Entry_t` per
  the naming scheme; its comparator params `a`/`b` are references and want the `r` prefix.
- `src/game/council/PlanetaryCouncil.cpp:425-441` — `RemoveActiveProposal_`/`ActivateProposal_`
  each `RebuildWorld` and `Bump`, so one resolution with N repeals does N+1 full rebuilds and
  bumps the revision N+3 times.
- `src/game/council/CouncilOutcomeApplier.cpp:46` — `rMembers` is unused (commented out) in
  `ApplyGovernor`; drop the parameter rather than leave the signature lying. Neither applier
  method mutates the applier — both should be `const`.
- `src/game/council/CouncilEffects.cpp:39-51` — `SetGovernorEffects` rebuilds the world wrappers
  too, because `RebuildWrappers_` does both lanes unconditionally.
- `src/game/council/CouncilEffects.cpp:68-70` — silently skipping a null `config` contradicts
  "throw on unexpected null"; `AppendActiveEffects` always sets it, so this hides a corruption.
- `src/game/council/CouncilAiStub.cpp:25`, `:43` — `if (!pMember)` guards are dead: the
  constructor rejects null members.
- `docs/architecture/council-system.md` documents four lifecycle phases but never mentions
  `CouncilAiStub`, the one component whose replacement it should describe.

**Observed outside slice:**
`src/ui/council/CouncilVoteView.cpp:52` — `Resolve` is called from a UI callback with no
try/catch, so any council precondition throw becomes an unhandled exception from the render loop.
`include/game/effects/ActiveEffect.h:32` — `ActiveEffect_t::config` has no default initializer,
so the null checks scattered around it are checking indeterminate values in any hand-built wrapper.
