#include "game/council/CouncilRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/council/CouncilEffects.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"

#include <algorithm>
#include <map>

namespace ac
{

namespace
{

bool HasContinuousWorldEffects_(const CouncilProposalConfig_t& rConfig)
{
    return std::any_of(rConfig.effects.begin(), rConfig.effects.end(),
                       IsContinuousWorldEffect);
}

Faction* FindMember_(const PlanetaryCouncil& rCouncil, FactionId_t factionId)
{
    for (Faction* pMember : rCouncil.Members())
    {
        if (pMember->GetFactionId() == factionId)
        {
            return pMember;
        }
    }
    return nullptr;
}

bool IsEligibleCandidate_(const PendingProposal_t& rPending, FactionId_t candidateId)
{
    const std::vector<FactionId_t>& rEligible = rPending.eligibleCandidateIds;
    return std::find(rEligible.begin(), rEligible.end(), candidateId) != rEligible.end();
}

// required_proposals asks "has this been enacted"; a one-shot like launch_solar_shade is
// history rather than a standing law, yet increase_solar_shade must still require it.
bool RequiredProposalsMet_(const PlanetaryCouncil& rCouncil,
                           const CouncilProposalConfig_t& rConfig)
{
    return std::all_of(rConfig.requiredProposals.begin(), rConfig.requiredProposals.end(),
                       [&](const std::string& rId)
                       { return rCouncil.HasPassed(rId) || rCouncil.IsActive(rId); });
}

// A repeal is meaningful only while something it repeals is in force, which is also what
// keeps it proposable again after its target is re-enacted.
bool RepealHasTarget_(const PlanetaryCouncil& rCouncil, const CouncilProposalConfig_t& rConfig)
{
    if (rConfig.repeals.empty())
    {
        return true;
    }
    return std::any_of(rConfig.repeals.begin(), rConfig.repeals.end(),
                       [&](const std::string& rId) { return rCouncil.IsActive(rId); });
}

bool RuleFlagGatesMet_(const PlanetaryCouncil& rCouncil, const CouncilProposalConfig_t& rConfig)
{
    for (const RuleFlagId_t flag : rConfig.requiresRuleFlags)
    {
        if (!rCouncil.HasActiveRuleFlag(flag))
        {
            return false;
        }
    }
    for (const RuleFlagId_t flag : rConfig.forbidsRuleFlags)
    {
        if (rCouncil.HasActiveRuleFlag(flag))
        {
            return false;
        }
    }
    return true;
}

// A passed non-repeatable proposal stays consumed. Continuous laws are exempt (a repeal clears
// them so they can be enacted again), and so are repeals, which track their target's return.
bool Consumed_(const PlanetaryCouncil& rCouncil, const CouncilProposalConfig_t& rConfig)
{
    if (rConfig.repeatable)
    {
        return false;
    }
    if (rCouncil.IsActive(rConfig.id))
    {
        return true;
    }
    return !HasContinuousWorldEffects_(rConfig) && rConfig.repeals.empty()
           && rCouncil.HasPassed(rConfig.id);
}

} // namespace

bool HasCommlinksToAllMembers(const PlanetaryCouncil& rCouncil, const Faction& rFaction)
{
    const DiplomacyLedger& rDiplomacy = CurrentGameState().GetDiplomacyLedger();
    const FactionId_t factionId = rFaction.GetFactionId();
    for (const Faction* pMember : rCouncil.Members())
    {
        const FactionId_t otherId = pMember->GetFactionId();
        if (otherId != factionId && !rDiplomacy.AreKnown(factionId, otherId))
        {
            return false;
        }
    }
    return true;
}

int ProposeCooldownYears(const PlanetaryCouncil& rCouncil, const Faction& rFaction)
{
    if (rCouncil.GetPlanetaryGovernor() == &rFaction)
    {
        return rCouncil.GetRules().governorProposeIntervalYears;
    }
    return rCouncil.GetRules().memberProposeIntervalYears;
}

int YearsUntilCanPropose(const PlanetaryCouncil& rCouncil, const Faction& rFaction)
{
    const std::optional<int> lastYear = rCouncil.LastProposedYear(rFaction);
    if (!lastYear)
    {
        return 0;
    }
    const int elapsed = CurrentGameState().GetMissionYear() - *lastYear;
    const int needed = ProposeCooldownYears(rCouncil, rFaction);
    return std::max(0, needed - elapsed);
}

bool CanPropose(const PlanetaryCouncil& rCouncil, const Faction& rProposer,
                const std::string& rProposalId)
{
    const CouncilProposalConfig_t* pConfig = rCouncil.GetRegistry().Find(rProposalId);
    if (!pConfig || !rCouncil.IsCouncilMember(rProposer) || !pConfig->proposable)
    {
        return false;
    }
    if (!pConfig->IsAvailable(rProposer.GetResearch().GetDiscoveredTechs()))
    {
        return false;
    }
    return RequiredProposalsMet_(rCouncil, *pConfig) && RepealHasTarget_(rCouncil, *pConfig)
           && RuleFlagGatesMet_(rCouncil, *pConfig) && !Consumed_(rCouncil, *pConfig);
}

int VoteWeight(const PlanetaryCouncil& rCouncil, const Faction& rFaction,
               CouncilVoteWeight_t mode)
{
    const double seed = mode == CouncilVoteWeight_t::Population
                            ? static_cast<double>(rFaction.TotalPopulation())
                            : 1.0;

    std::vector<ActiveEffect_t> effects = rFaction.GetLocalActiveEffects().effects;
    const std::vector<ActiveEffect_t>& rFactionExtras = rCouncil.CollectFactionEffects(rFaction);
    effects.insert(effects.end(), rFactionExtras.begin(), rFactionExtras.end());
    const std::vector<ActiveEffect_t>& rWorld = rCouncil.CollectWorldEffects();
    effects.insert(effects.end(), rWorld.begin(), rWorld.end());

    const StatBreakdown_t breakdown =
        ResolveStatModifiers(FilterByStatId(effects, StatId_t::CouncilVotes), seed);
    return std::max(0, FinalizeResolvedStat(breakdown.total));
}

std::vector<Faction*> GovernorCandidates(const PlanetaryCouncil& rCouncil)
{
    struct Entry_t
    {
        Faction* pFaction;
        int pop;
    };
    std::vector<Entry_t> ranked;
    for (Faction* pFaction : rCouncil.Members())
    {
        ranked.push_back({pFaction, pFaction->TotalPopulation()});
    }
    std::sort(ranked.begin(), ranked.end(), [](const Entry_t& a, const Entry_t& b)
    {
        if (a.pop != b.pop)
        {
            return a.pop > b.pop;
        }
        return a.pFaction->GetFactionId() < b.pFaction->GetFactionId();
    });

    std::vector<Faction*> result;
    for (size_t i = 0; i < ranked.size() && result.size() < 2; ++i)
    {
        result.push_back(ranked[i].pFaction);
    }
    return result;
}

std::vector<Faction*> EligibleCandidates(const PlanetaryCouncil& rCouncil,
                                         const CouncilProposalConfig_t& rConfig)
{
    if (rConfig.electionOutcome == CouncilElectionOutcome_t::PlanetaryGovernor)
    {
        return GovernorCandidates(rCouncil);
    }
    return rCouncil.Members();
}

bool ProposalPasses(const PlanetaryCouncil& rCouncil, const PendingProposal_t& rPending)
{
    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(rPending.proposalId);
    int yea = 0;
    int nay = 0;
    int totalWeight = 0;
    for (Faction* pMember : rCouncil.Members())
    {
        const int weight = VoteWeight(rCouncil, *pMember, rConfig.voteWeight);
        totalWeight += weight;
        const auto it = rPending.ballots.find(pMember->GetFactionId());
        if (it == rPending.ballots.end() || it->second == CouncilBallot_t::Abstain)
        {
            continue;
        }
        if (it->second == CouncilBallot_t::Yea)
        {
            yea += weight;
        }
        else
        {
            nay += weight;
        }
    }

    if (rConfig.voteThreshold > 0.0)
    {
        const double share = totalWeight > 0 ? static_cast<double>(yea) / totalWeight : 0.0;
        return share >= rConfig.voteThreshold;
    }
    return yea > nay;
}

Faction* ElectionWinner(const PlanetaryCouncil& rCouncil, const PendingProposal_t& rPending)
{
    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(rPending.proposalId);
    std::map<FactionId_t, int> tallies;
    int totalWeight = 0;
    for (Faction* pMember : rCouncil.Members())
    {
        const int weight = VoteWeight(rCouncil, *pMember, rConfig.voteWeight);
        totalWeight += weight;
        const auto it = rPending.electionVotes.find(pMember->GetFactionId());
        if (it == rPending.electionVotes.end() || !it->second)
        {
            continue;
        }
        // Checked at tally time as well as at cast time: this is the last point before a
        // winner is installed.
        if (!FindMember_(rCouncil, *it->second) || !IsEligibleCandidate_(rPending, *it->second))
        {
            continue;
        }
        tallies[*it->second] += weight;
    }

    // Keyed in ascending faction id, so a strict > keeps the lowest id on a tie.
    FactionId_t bestId = 0;
    int bestVotes = -1;
    for (const auto& [id, votes] : tallies)
    {
        if (votes > bestVotes)
        {
            bestVotes = votes;
            bestId = id;
        }
    }
    if (bestVotes <= 0)
    {
        return nullptr;
    }
    if (rConfig.voteThreshold > 0.0)
    {
        const double share =
            totalWeight > 0 ? static_cast<double>(bestVotes) / totalWeight : 0.0;
        if (share < rConfig.voteThreshold)
        {
            return nullptr;
        }
    }
    return FindMember_(rCouncil, bestId);
}

bool CanVeto(const PlanetaryCouncil& rCouncil, const Faction& rFaction,
             const PendingProposal_t& rPending)
{
    if (rCouncil.GetPlanetaryGovernor() != &rFaction)
    {
        return false;
    }
    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(rPending.proposalId);
    return rConfig.kind != CouncilProposalKind_t::Election;
}

bool VetoOverruled(const PlanetaryCouncil& rCouncil, const PendingProposal_t& rPending)
{
    if (!rPending.vetoed)
    {
        return false;
    }
    for (const Faction* pMember : rCouncil.Members())
    {
        if (pMember == rCouncil.GetPlanetaryGovernor())
        {
            continue;
        }
        const auto it = rPending.ballots.find(pMember->GetFactionId());
        if (it == rPending.ballots.end() || it->second != CouncilBallot_t::Yea)
        {
            return false;
        }
    }
    return true;
}

} // namespace ac
