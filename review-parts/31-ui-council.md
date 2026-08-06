## UI — council vote

**Files:** `src/ui/council/CouncilBallotPopup.cpp`, `include/ui/council/CouncilBallotPopup.h`,
`src/ui/council/CouncilFactionVotesPanel.cpp`, `include/ui/council/CouncilFactionVotesPanel.h`,
`src/ui/council/CouncilProposalInfoPanel.cpp`, `include/ui/council/CouncilProposalInfoPanel.h`,
`src/ui/council/CouncilVoteButton.cpp`, `include/ui/council/CouncilVoteButton.h`,
`src/ui/council/CouncilVoteView.cpp`, `include/ui/council/CouncilVoteView.h`

**Assessment:** The view is a clear coordinator — top member columns, center proposal/tally,
Vote button, and a mode-specific ballot popup — and it correctly resolves only when
`AllMembersVoted()`. Layout and colors for the panels come from `councilVoteView` style. The
dominant weakness is lifecycle: Escape (and silent no-ops) can leave a pending proposal with no
UI re-entry, and election candidates / per-frame weight work are not tied to the council’s own
APIs (`GovernorCandidates`, `GetRevision`).

### [H] Do not dismiss the vote view while a proposal is still pending
`src/ui/council/CouncilVoteView.cpp:37-40` — Escape sets `m_bShouldClose` without casting or
calling `Resolve`. With AI ballots cast on `OnProposalOpened` (`Engine.cpp:335-340` →
`CastStubCouncilVotes`), only the player remains; if they Escape before voting, `m_pending`
stays set. `CommlinksView::OnProposalSelected_` refuses a new `Propose` while pending
(`src/ui/commlinks/CommlinksView.cpp:96-98`), and `CreateCouncilVoteView` is only pushed after a
successful Propose — so the council is bricked for the rest of the game. Fix: refuse Escape
(or treat it as abstain + resolve) until there is no pending proposal; do not close the overlay
on an open vote.

### [M] Election ballot lists every member, not eligible governor candidates
`src/ui/council/CouncilVoteView.cpp:69-73` — `CreateElection` always gets `pCouncil->Members()`.
For `electionOutcome == PlanetaryGovernor`, architecture and `GovernorCandidates()` restrict the
field to the two most populous members; `CastElectionVote` currently does not enforce that
either (sibling council-runtime finding). The UI is the surface that decides what the player can
pick, so ineligible winners are one click away. Pass `GovernorCandidates()` (or a council helper
keyed on `electionOutcome`) instead of the full membership for governor elections.

### [M] Recompute vote weights every frame with no revision cache
`src/ui/council/CouncilFactionVotesPanel.cpp:119` and
`src/ui/council/CouncilProposalInfoPanel.cpp:71,121` — both `Render` paths call
`ComputeVoteWeight` per member every frame. That copies the faction effect pool and runs
`ResolveStatModifiers` (`PlanetaryCouncil.cpp:163-175`). The council already exposes
`GetRevision()` for pull-based UI, but neither panel caches weights/tallies against it. Cache
display state on council revision (and invalidate when ballots change) instead of resolving
stats on paint.

### [M] Vote can stack multiple ballot popups
`src/ui/council/CouncilVoteView.cpp:26-28,71,86` — each Vote click `push_back`s a new
`CouncilBallotPopup`. The popup uses `popupSmall` and does not cover the Vote button; hit-test
is topmost-first (`IGameView::HandleMouse`), so a click on Vote while a popup is open adds
another chooser. Guard with a single open selector (or disable Vote while one exists).

### [M] Silent no-ops when council or player is missing
`src/ui/council/CouncilVoteView.cpp:47-50,58-62,77-80,91-94` — `TryResolveAndClose_`,
`OpenBallotSelector_`, and the cast lambdas return quietly if `GetPlanetaryCouncil()` or
`GetPlayerFaction()` is null. This view only exists for an active council vote; those pointers
being null is unexpected. Per project guidelines, throw rather than leave the player staring at
a Vote button that does nothing. Same pattern in the panel `Render` early returns
(`CouncilFactionVotesPanel.cpp:83-91`, `CouncilProposalInfoPanel.cpp:31-39`) — empty chrome with
no diagnostic.

### [M] `Resolve` from the cast callback is uncaught
`src/ui/council/CouncilVoteView.cpp:52,80,94` — `Resolve` throws if preconditions fail (e.g. not
all members voted). The callback has no try/catch, so a council invariant failure becomes an
unhandled exception out of the input/render loop. Catch and surface, or assert the
`AllMembersVoted()` gate so a throw is truly unreachable.

### [L] Convention and hygiene items
- `src/ui/council/CouncilFactionVotesPanel.cpp:17-28` — `BallotLabel_` hand-rolls Yea/Nay/Abstain; enumerator names match display strings, so prefer `magic_enum` (same labels duplicated at `CouncilBallotPopup.cpp:40-41`).
- `src/ui/council/CouncilBallotPopup.cpp:81,103` — ballot chrome uses `Style().productionSelectorPopup` instead of a council-specific style block.
- `src/ui/council/CouncilVoteButton.cpp:16` — Vote button uses `Style().commlinksButton` while panels use `councilVoteView`.
- `src/ui/council/CouncilFactionVotesPanel.cpp:109` / `CouncilProposalInfoPanel.cpp:67` — null-member guards are dead; `PlanetaryCouncil` rejects null members at construction.
- No tests exercise Escape-abandon, election candidate filtering, or the vote→resolve UI path (game tests cover council rules only).

**Observed outside slice:**
`src/ui/commlinks/CommlinksView.cpp:96-116` — only entry to `CouncilVoteView` is post-Propose; no path reopens an existing pending vote (amplifies the Escape finding).
`src/game/council/PlanetaryCouncil.cpp:353-357` — `CastElectionVote` accepts any member as candidate, so UI filtering alone is not a hard rule until the council validates too.
