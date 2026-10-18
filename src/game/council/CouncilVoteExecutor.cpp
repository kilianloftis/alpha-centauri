#include "game/council/CouncilVoteExecutor.h"

#include "game/GameState.h"
#include "game/council/CouncilMutations.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRules.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/faction/Faction.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

CouncilVoteExecutor::CouncilVoteExecutor(PlanetaryCouncil& rCouncil)
    : m_rCouncil(rCouncil)
{
}

void CouncilVoteExecutor::Propose(GameState& rGameState, Faction& rProposer,
                                  const std::string& rProposalId)
{
    if (m_pending)
    {
        throw std::invalid_argument("CouncilVoteExecutor::Propose: proposal already pending");
    }
    if (!CanPropose(m_rCouncil, rProposer, rProposalId))
    {
        throw std::invalid_argument(
            "CouncilVoteExecutor::Propose: proposal not available '" + rProposalId + "'");
    }
    if (!HasCommlinksToAllMembers(m_rCouncil, rProposer))
    {
        throw std::invalid_argument(
            "CouncilVoteExecutor::Propose: missing commlinks to all members");
    }
    if (YearsUntilCanPropose(m_rCouncil, rProposer) > 0)
    {
        throw std::invalid_argument("CouncilVoteExecutor::Propose: on cooldown");
    }

    std::vector<FactionId_t> memberIds;
    memberIds.reserve(m_rCouncil.Members().size());
    for (const Faction* pMember : m_rCouncil.Members())
    {
        memberIds.push_back(pMember->GetFactionId());
    }
    rGameState.GetDiplomacyLedger().SetKnown(memberIds);

    PendingProposal_t pending;
    pending.proposalId = rProposalId;
    pending.proposerId = rProposer.GetFactionId();
    const CouncilProposalConfig_t& rConfig = m_rCouncil.GetRegistry().Get(rProposalId);
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        for (const Faction* pCandidate : EligibleCandidates(m_rCouncil, rConfig))
        {
            pending.eligibleCandidateIds.push_back(pCandidate->GetFactionId());
        }
    }
    m_pending = std::move(pending);
    m_rCouncil.RecordProposal(rProposer.GetFactionId(), rGameState.GetMissionYear());

    // An observer that throws must not strand the pending slot, which only Resolve clears.
    try
    {
        OnProposalOpened.Emit(rProposer, rProposalId);
    }
    catch (...)
    {
        m_pending.reset();
        throw;
    }
}

void CouncilVoteExecutor::RequireMemberAndPending_(const Faction& rVoter,
                                                   const char* pCaller) const
{
    if (!m_pending)
    {
        throw std::invalid_argument(std::string(pCaller) + ": no pending proposal");
    }
    if (!m_rCouncil.IsCouncilMember(rVoter))
    {
        throw std::invalid_argument(std::string(pCaller) + ": voter is not a council member");
    }
}

void CouncilVoteExecutor::CastVote(const Faction& rVoter, CouncilBallot_t ballot)
{
    RequireMemberAndPending_(rVoter, "CouncilVoteExecutor::CastVote");
    const CouncilProposalConfig_t& rConfig = m_rCouncil.GetRegistry().Get(m_pending->proposalId);
    if (rConfig.kind != CouncilProposalKind_t::Standard)
    {
        throw std::invalid_argument(
            "CouncilVoteExecutor::CastVote: pending proposal is not a standard ballot");
    }
    m_pending->ballots[rVoter.GetFactionId()] = ballot;
}

void CouncilVoteExecutor::CastElectionVote(const Faction& rVoter, const Faction* pCandidate)
{
    RequireMemberAndPending_(rVoter, "CouncilVoteExecutor::CastElectionVote");
    const CouncilProposalConfig_t& rConfig = m_rCouncil.GetRegistry().Get(m_pending->proposalId);
    if (rConfig.kind != CouncilProposalKind_t::Election)
    {
        throw std::invalid_argument(
            "CouncilVoteExecutor::CastElectionVote: pending proposal is not an election");
    }
    if (pCandidate && !m_rCouncil.IsCouncilMember(*pCandidate))
    {
        throw std::invalid_argument(
            "CouncilVoteExecutor::CastElectionVote: candidate is not a council member");
    }
    const std::vector<FactionId_t>& rEligible = m_pending->eligibleCandidateIds;
    if (pCandidate
        && std::find(rEligible.begin(), rEligible.end(), pCandidate->GetFactionId())
               == rEligible.end())
    {
        throw std::invalid_argument(
            "CouncilVoteExecutor::CastElectionVote: '" + pCandidate->GetDefinitionId()
            + "' is not an eligible candidate for '" + rConfig.id + "'");
    }
    std::optional<FactionId_t> candidateId;
    if (pCandidate)
    {
        candidateId = pCandidate->GetFactionId();
    }
    m_pending->electionVotes[rVoter.GetFactionId()] = candidateId;
}

bool CouncilVoteExecutor::VetoPending(const Faction& rGovernor)
{
    if (!m_pending || !CanVeto(m_rCouncil, rGovernor, *m_pending))
    {
        return false;
    }
    m_pending->vetoed = true;
    return true;
}

bool CouncilVoteExecutor::AllMembersVoted() const
{
    if (!m_pending)
    {
        return false;
    }
    const CouncilProposalConfig_t& rConfig = m_rCouncil.GetRegistry().Get(m_pending->proposalId);
    const bool bElection = rConfig.kind == CouncilProposalKind_t::Election;
    for (const Faction* pMember : m_rCouncil.Members())
    {
        const FactionId_t id = pMember->GetFactionId();
        const bool bVoted = bElection ? m_pending->electionVotes.contains(id)
                                      : m_pending->ballots.contains(id);
        if (!bVoted)
        {
            return false;
        }
    }
    return true;
}

ResolveProposalResult_t CouncilVoteExecutor::ApplyPassed_(GameState& rGameState,
                                                          Faction* pElectionWinner,
                                                          bool bOverruled)
{
    const CouncilProposalConfig_t& rConfig = m_rCouncil.GetRegistry().Get(m_pending->proposalId);
    m_rCouncil.RecordPassed(rConfig);
    ApplyPassedCouncilEffects(rGameState, m_rCouncil.Members(), rConfig);
    m_rCouncil.Enact(rConfig);

    ResolveProposalResult_t result =
        bOverruled ? ResolveProposalResult_t::VetoOverruled : ResolveProposalResult_t::Passed;
    if (pElectionWinner == nullptr)
    {
        return result;
    }
    if (rConfig.electionOutcome == CouncilElectionOutcome_t::PlanetaryGovernor)
    {
        m_rCouncil.InstallGovernor(*pElectionWinner);
        ApplyCouncilGovernor(rGameState, *pElectionWinner, m_rCouncil.GetRules());
    }
    else if (rConfig.electionOutcome == CouncilElectionOutcome_t::SupremeLeaderVictory)
    {
        m_rCouncil.SetSupremeLeaderVictory(*pElectionWinner);
        result = ResolveProposalResult_t::SupremeLeaderVictoryStub;
    }
    return result;
}

ResolveProposalResult_t CouncilVoteExecutor::ResolveOutcome_(GameState& rGameState)
{
    const bool bOverruled = VetoOverruled(m_rCouncil, *m_pending);
    if (m_pending->vetoed && !bOverruled)
    {
        return ResolveProposalResult_t::Vetoed;
    }

    const CouncilProposalConfig_t& rConfig = m_rCouncil.GetRegistry().Get(m_pending->proposalId);
    Faction* pElectionWinner = nullptr;
    bool bPassed = false;
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        pElectionWinner = ElectionWinner(m_rCouncil, *m_pending);
        bPassed = pElectionWinner != nullptr;
    }
    else
    {
        bPassed = ProposalPasses(m_rCouncil, *m_pending);
    }
    if (!bPassed)
    {
        return ResolveProposalResult_t::Failed;
    }
    return ApplyPassed_(rGameState, pElectionWinner, bOverruled);
}

ResolveProposalResult_t CouncilVoteExecutor::Resolve(GameState& rGameState)
{
    if (!m_pending)
    {
        throw std::invalid_argument("CouncilVoteExecutor::Resolve: no pending proposal");
    }
    const std::string proposalId = m_pending->proposalId;
    const ResolveProposalResult_t result = ResolveOutcome_(rGameState);
    m_pending.reset();
    OnResolved.Emit(proposalId, result);
    return result;
}

void CouncilVoteExecutor::Expel(const Faction& rFaction)
{
    if (!m_rCouncil.IsCouncilMember(rFaction))
    {
        return;
    }
    m_rCouncil.Expel(rFaction);
    if (m_pending)
    {
        const FactionId_t id = rFaction.GetFactionId();
        m_pending->ballots.erase(id);
        m_pending->electionVotes.erase(id);
        std::erase(m_pending->eligibleCandidateIds, id);
    }
}

} // namespace ac
