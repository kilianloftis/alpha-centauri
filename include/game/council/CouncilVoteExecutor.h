#pragma once

#include "game/council/PendingProposal.h"
#include "lib/Signal.h"

#include <optional>
#include <string>

namespace ac
{

class Faction;
class GameState;
class PlanetaryCouncil;

// Outcome of a successfully resolved proposal (precondition failures throw).
enum class ResolveProposalResult_t
{
    Passed,
    Failed,
    Vetoed,
    VetoOverruled,
    // Supreme Leader election reached the threshold; victory stub recorded.
    SupremeLeaderVictoryStub,
};

// Runs a council vote: Propose opens it (there is no separate convene stage), members cast
// ballots, Resolve tallies it through CouncilRules and applies the outcome. GameState owns it
// beside the council it acts on.
class CouncilVoteExecutor
{
public:
    explicit CouncilVoteExecutor(PlanetaryCouncil& rCouncil);

    const PendingProposal_t* GetPending() const { return m_pending ? &*m_pending : nullptr; }

    // Checks availability, commlinks and cooldown; shares commlinks among members; starts the
    // proposer's cooldown; opens the vote and tells observers.
    void Propose(GameState& rGameState, Faction& rProposer, const std::string& rProposalId);

    void CastVote(const Faction& rVoter, CouncilBallot_t ballot);
    // pCandidate null = abstain. The candidate must be in the snapshot taken at Propose.
    void CastElectionVote(const Faction& rVoter, const Faction* pCandidate);

    // Returns false when nothing is pending or rGovernor may not veto it.
    bool VetoPending(const Faction& rGovernor);

    // True when every council member has a ballot (or election vote) on the pending proposal.
    bool AllMembersVoted() const;

    // A member that never voted abstains, so the vote can always be resolved.
    ResolveProposalResult_t Resolve(GameState& rGameState);

    // Expels rFaction from the council and drops whatever it cast on the pending vote, so the
    // tally weighs only seats that still exist.
    void Expel(const Faction& rFaction);

    // The UI opens the vote popup and AI observers decide their ballots. Read GetPending().
    Signal<Faction&, const std::string&> OnProposalOpened;
    // Fired once a pending proposal resolves, for every outcome (proposalId, result).
    Signal<const std::string&, ResolveProposalResult_t> OnResolved;

private:
    void RequireMemberAndPending_(const Faction& rVoter, const char* pCaller) const;
    ResolveProposalResult_t ResolveOutcome_(GameState& rGameState);
    ResolveProposalResult_t ApplyPassed_(GameState& rGameState, Faction* pElectionWinner,
                                         bool bOverruled);

    PlanetaryCouncil& m_rCouncil;
    std::optional<PendingProposal_t> m_pending;
};

} // namespace ac
