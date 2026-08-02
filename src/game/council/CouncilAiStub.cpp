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
        for (Faction* pMember : rCouncil.Members())
        {
            if (pMember && pMember->GetFactionId() == pPending->proposerId)
            {
                pElectionChoice = pMember;
                break;
            }
        }
        if (!pElectionChoice)
        {
            const std::vector<Faction*> candidates = rCouncil.GovernorCandidates();
            if (!candidates.empty())
            {
                pElectionChoice = candidates.front();
            }
        }
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
