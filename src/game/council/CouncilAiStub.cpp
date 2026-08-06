#include "game/council/CouncilAiStub.h"
#include "game/Faction.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/PlanetaryCouncil.h"

namespace ac
{

void CastStubCouncilVotes(PlanetaryCouncil& rCouncil)
{
    const PlanetaryCouncil::PendingProposal_t* pPending = rCouncil.GetPending();
    if (!pPending)
    {
        return;
    }

    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(pPending->proposalId);

    const Faction* pElectionChoice = nullptr;
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        // Ask the council who may be voted for rather than guessing. Backing the proposer is
        // the stub's preference, but the proposer need not be eligible — nothing stops a
        // faction outside the top two from proposing a governor election — and
        // CastElectionVote rejects an ineligible candidate. Since this runs inside Propose's
        // OnProposalOpened emission, a throw here would unwind out of Propose with m_pending
        // already set: the council would be left holding a vote nothing can resolve.
        const std::vector<Faction*> eligible = rCouncil.EligibleCandidates(rConfig);
        for (Faction* pCandidate : eligible)
        {
            if (pCandidate && pCandidate->GetFactionId() == pPending->proposerId)
            {
                pElectionChoice = pCandidate;
                break;
            }
        }
        if (!pElectionChoice && !eligible.empty())
        {
            pElectionChoice = eligible.front();
        }
        // Leaving pElectionChoice null is a valid abstention if nobody is eligible.
    }

    for (Faction* pMember : rCouncil.Members())
    {
        if (!pMember || pMember->IsPlayerControlled())
        {
            continue;
        }

        const FactionId_t id = pMember->GetFactionId();
        if (rConfig.kind == CouncilProposalKind_t::Election)
        {
            if (pPending->electionVotes.find(id) != pPending->electionVotes.end())
            {
                continue;
            }
            rCouncil.CastElectionVote(*pMember, pElectionChoice);
        }
        else
        {
            if (pPending->ballots.find(id) != pPending->ballots.end())
            {
                continue;
            }
            rCouncil.CastVote(*pMember, CouncilBallot_t::Yea);
        }
    }
}

} // namespace ac
