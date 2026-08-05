#include "game/council/PlanetaryCouncil.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/ResearchManager.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

bool HasContinuousWorldEffects_(const CouncilProposalConfig_t& rConfig)
{
    return std::any_of(rConfig.effects.begin(), rConfig.effects.end(),
                       IsContinuousWorldEffect);
}

} // namespace

PlanetaryCouncil::PlanetaryCouncil(const CouncilProposalRegistry& rRegistry,
                                   const CouncilRulesConfig_t& rRules,
                                   std::vector<Faction*> members)
    : m_rRegistry(rRegistry)
    , m_rRules(rRules)
    , m_members(std::move(members))
    , m_applier(rRules)
{
    if (m_members.empty())
    {
        throw std::invalid_argument("PlanetaryCouncil: members must not be empty");
    }
    for (Faction* pMember : m_members)
    {
        if (!pMember)
        {
            throw std::invalid_argument("PlanetaryCouncil: member pointer is null");
        }
        if (!pMember->GetDefinition().identity.participatesInCouncil)
        {
            throw std::invalid_argument(
                "PlanetaryCouncil: faction does not participate in the council");
        }
    }
    for (size_t i = 0; i < m_members.size(); ++i)
    {
        for (size_t j = i + 1; j < m_members.size(); ++j)
        {
            if (m_members[i]->GetFactionId() == m_members[j]->GetFactionId())
            {
                throw std::invalid_argument("PlanetaryCouncil: duplicate member faction");
            }
        }
    }
    ActivateInitiallyActiveProposals_();
}

void PlanetaryCouncil::ActivateInitiallyActiveProposals_()
{
    for (const CouncilProposalConfig_t& rConfig : m_rRegistry.GetAll())
    {
        if (rConfig.initiallyActive)
        {
            ActivateProposal_(rConfig);
        }
    }
}

Faction* PlanetaryCouncil::FindMember_(FactionId_t factionId) const
{
    for (Faction* pMember : m_members)
    {
        if (pMember->GetFactionId() == factionId)
        {
            return pMember;
        }
    }
    return nullptr;
}

bool PlanetaryCouncil::IsCouncilMember(const Faction& rFaction) const
{
    return std::find(m_members.begin(), m_members.end(), &rFaction) != m_members.end();
}

bool PlanetaryCouncil::HasCommlinksToAllMembers(const GameState& rGameState,
                                                const Faction& rFaction) const
{
    const DiplomacyLedger& rDiplomacy = rGameState.GetDiplomacyLedger();
    const FactionId_t factionId = rFaction.GetFactionId();
    for (const Faction* pMember : m_members)
    {
        const FactionId_t otherId = pMember->GetFactionId();
        if (otherId == factionId)
        {
            continue;
        }
        if (!rDiplomacy.AreKnown(factionId, otherId))
        {
            return false;
        }
    }
    return true;
}

int PlanetaryCouncil::ProposeCooldownYears(const Faction& rFaction) const
{
    if (m_pGovernor == &rFaction)
    {
        return m_rRules.governorProposeIntervalYears;
    }
    return m_rRules.memberProposeIntervalYears;
}

int PlanetaryCouncil::YearsUntilCanPropose(const GameState& rGameState,
                                          const Faction& rFaction) const
{
    const auto it = m_lastProposedYear.find(rFaction.GetFactionId());
    if (it == m_lastProposedYear.end())
    {
        return 0;
    }
    const int elapsed = rGameState.GetMissionYear() - it->second;
    const int needed = ProposeCooldownYears(rFaction);
    return std::max(0, needed - elapsed);
}

std::optional<int> PlanetaryCouncil::LastProposedYear(const Faction& rFaction) const
{
    const auto it = m_lastProposedYear.find(rFaction.GetFactionId());
    if (it == m_lastProposedYear.end())
    {
        return std::nullopt;
    }
    return it->second;
}

bool PlanetaryCouncil::IsActive(const std::string& rProposalId) const
{
    return std::find(m_activeProposalIds.begin(), m_activeProposalIds.end(), rProposalId)
           != m_activeProposalIds.end();
}

bool PlanetaryCouncil::HasActiveRuleFlag(RuleFlagId_t flag) const
{
    return m_effects.HasActiveRuleFlag(flag);
}

int PlanetaryCouncil::ComputeVoteWeight(const Faction& rFaction,
                                        CouncilVoteWeight_t mode) const
{
    const double seed = (mode == CouncilVoteWeight_t::Population)
                            ? static_cast<double>(rFaction.TotalPopulation())
                            : 1.0;

    std::vector<ActiveEffect_t> effects = rFaction.GetLocalActiveEffects().effects;
    for (const ActiveEffect_t& rExtra : CollectFactionEffects(rFaction))
    {
        effects.push_back(rExtra);
    }
    for (const ActiveEffect_t& rWorld : CollectWorldEffects())
    {
        effects.push_back(rWorld);
    }

    const StatBreakdown_t breakdown =
        ResolveStatModifiers(FilterByStatId(effects, StatId_t::CouncilVotes), seed);
    return std::max(0, FinalizeResolvedStat(breakdown.total));
}

std::vector<Faction*> PlanetaryCouncil::GovernorCandidates() const
{
    struct Entry
    {
        Faction* pFaction;
        int pop;
    };
    std::vector<Entry> ranked;
    for (Faction* pFaction : m_members)
    {
        ranked.push_back({pFaction, pFaction->TotalPopulation()});
    }
    std::sort(ranked.begin(), ranked.end(), [](const Entry& a, const Entry& b)
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

bool PlanetaryCouncil::CanPropose(const Faction& rProposer, const std::string& rProposalId) const
{
    const CouncilProposalConfig_t* pConfig = m_rRegistry.Find(rProposalId);
    if (!pConfig)
    {
        return false;
    }

    if (!IsCouncilMember(rProposer))
    {
        return false;
    }
    if (!pConfig->proposable)
    {
        return false;
    }

    if (!pConfig->IsAvailable(rProposer.GetResearch().GetDiscoveredTechs()))
    {
        return false;
    }

    for (const std::string& rRequired : pConfig->requiredProposals)
    {
        if (!IsActive(rRequired))
        {
            return false;
        }
    }

    for (const RuleFlagId_t flag : pConfig->requiresRuleFlags)
    {
        if (!HasActiveRuleFlag(flag))
        {
            return false;
        }
    }
    for (const RuleFlagId_t flag : pConfig->forbidsRuleFlags)
    {
        if (HasActiveRuleFlag(flag))
        {
            return false;
        }
    }

    if (IsActive(rProposalId) && !pConfig->repeatable)
    {
        return false;
    }

    // A non-repeatable proposal that has already passed stays consumed. The active-id check
    // above covers passes that leave a lasting marker; this catches passes that don't — one
    // whose marker was later removed by a repeal. Continuous world laws are exempt: repealing
    // one clears its active id precisely so it can be enacted again.
    if (!pConfig->repeatable && !IsActive(rProposalId) && !HasContinuousWorldEffects_(*pConfig))
    {
        const auto it = m_passCounts.find(rProposalId);
        if (it != m_passCounts.end() && it->second > 0)
        {
            return false;
        }
    }

    return true;
}

void PlanetaryCouncil::Propose(GameState& rGameState, Faction& rProposer,
                               const std::string& rProposalId)
{
    if (m_pending)
    {
        throw std::invalid_argument("PlanetaryCouncil::Propose: proposal already pending");
    }
    // CanPropose owns eligibility (membership, a known and available proposal, tech, gates);
    // commlinks and cooldown are the propose-time gates it deliberately leaves out.
    if (!CanPropose(rProposer, rProposalId))
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::Propose: proposal not available '" + rProposalId + "'");
    }
    if (!HasCommlinksToAllMembers(rGameState, rProposer))
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::Propose: missing commlinks to all members");
    }
    if (YearsUntilCanPropose(rGameState, rProposer) > 0)
    {
        throw std::invalid_argument("PlanetaryCouncil::Propose: on cooldown");
    }

    std::vector<FactionId_t> memberIds;
    memberIds.reserve(m_members.size());
    for (Faction* pMember : m_members)
    {
        memberIds.push_back(pMember->GetFactionId());
    }
    rGameState.GetDiplomacyLedger().SetKnown(memberIds);

    PendingProposal_t pending;
    pending.proposalId = rProposalId;
    pending.proposerId = rProposer.GetFactionId();
    m_pending = std::move(pending);
    m_lastProposedYear[rProposer.GetFactionId()] = rGameState.GetMissionYear();
    m_revision.Bump();
    OnProposalOpened.Emit(rProposer, rProposalId);
}

void PlanetaryCouncil::CastVote(const Faction& rVoter, CouncilBallot_t ballot)
{
    if (!m_pending)
    {
        throw std::invalid_argument("PlanetaryCouncil::CastVote: no pending proposal");
    }
    if (!IsCouncilMember(rVoter))
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::CastVote: voter is not a council member");
    }
    const CouncilProposalConfig_t& rConfig = m_rRegistry.Get(m_pending->proposalId);
    if (rConfig.kind != CouncilProposalKind_t::Standard)
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::CastVote: pending proposal is not a standard ballot");
    }
    m_pending->ballots[rVoter.GetFactionId()] = ballot;
    m_revision.Bump();
}

void PlanetaryCouncil::CastElectionVote(const Faction& rVoter, const Faction* pCandidate)
{
    if (!m_pending)
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::CastElectionVote: no pending proposal");
    }
    if (!IsCouncilMember(rVoter))
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::CastElectionVote: voter is not a council member");
    }
    const CouncilProposalConfig_t& rConfig = m_rRegistry.Get(m_pending->proposalId);
    if (rConfig.kind != CouncilProposalKind_t::Election)
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::CastElectionVote: pending proposal is not an election");
    }
    if (pCandidate && !IsCouncilMember(*pCandidate))
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::CastElectionVote: candidate is not a council member");
    }
    std::optional<FactionId_t> candidateId;
    if (pCandidate)
    {
        candidateId = pCandidate->GetFactionId();
    }
    m_pending->electionVotes[rVoter.GetFactionId()] = candidateId;
    m_revision.Bump();
}

bool PlanetaryCouncil::VetoPending(const Faction& rGovernor)
{
    if (!m_pending || m_pGovernor != &rGovernor)
    {
        return false;
    }
    m_pending->vetoed = true;
    m_revision.Bump();
    return true;
}

bool PlanetaryCouncil::VetoUnanimouslyOverruled_() const
{
    if (!m_pending || !m_pending->vetoed)
    {
        return false;
    }
    // A veto stands unless every council member other than the governor voted Yea.
    for (const Faction* pMember : m_members)
    {
        if (pMember == m_pGovernor)
        {
            continue;
        }
        const auto it = m_pending->ballots.find(pMember->GetFactionId());
        if (it == m_pending->ballots.end() || it->second != CouncilBallot_t::Yea)
        {
            return false;
        }
    }
    return true;
}

bool PlanetaryCouncil::AllMembersVoted() const
{
    if (!m_pending)
    {
        return false;
    }
    const CouncilProposalConfig_t& rConfig = m_rRegistry.Get(m_pending->proposalId);
    for (const Faction* pMember : m_members)
    {
        if (rConfig.kind == CouncilProposalKind_t::Election)
        {
            if (m_pending->electionVotes.find(pMember->GetFactionId())
                == m_pending->electionVotes.end())
            {
                return false;
            }
        }
        else if (m_pending->ballots.find(pMember->GetFactionId()) == m_pending->ballots.end())
        {
            return false;
        }
    }
    return true;
}

void PlanetaryCouncil::RemoveActiveProposal_(const std::string& rProposalId)
{
    m_activeProposalIds.erase(
        std::remove(m_activeProposalIds.begin(), m_activeProposalIds.end(), rProposalId),
        m_activeProposalIds.end());
    m_effects.RebuildWorld(m_activeProposalIds, m_rRegistry);
    m_revision.Bump();
}

void PlanetaryCouncil::ActivateProposal_(const CouncilProposalConfig_t& rConfig)
{
    if (!IsActive(rConfig.id))
    {
        m_activeProposalIds.push_back(rConfig.id);
    }
    m_effects.RebuildWorld(m_activeProposalIds, m_rRegistry);
    m_revision.Bump();
}

void PlanetaryCouncil::ApplyGovernor_(GameState& rGameState, Faction& rGovernor)
{
    m_pGovernor = &rGovernor;
    m_effects.SetGovernorEffects(m_rRules.governorEffects);
    m_revision.Bump();
    m_applier.ApplyGovernor(rGameState, m_members, rGovernor);
}

bool PlanetaryCouncil::TallyStandard_(const CouncilProposalConfig_t& rConfig) const
{
    int yea = 0;
    int nay = 0;
    for (Faction* pMember : m_members)
    {
        const int weight = ComputeVoteWeight(*pMember, rConfig.voteWeight);
        const auto it = m_pending->ballots.find(pMember->GetFactionId());
        if (it == m_pending->ballots.end() || it->second == CouncilBallot_t::Abstain)
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
    return yea > nay;
}

Faction* PlanetaryCouncil::TallyElection_(const CouncilProposalConfig_t& rConfig) const
{
    std::map<FactionId_t, int> tallies;
    int totalWeight = 0;
    for (Faction* pMember : m_members)
    {
        const int weight = ComputeVoteWeight(*pMember, rConfig.voteWeight);
        totalWeight += weight;
        const auto it = m_pending->electionVotes.find(pMember->GetFactionId());
        if (it == m_pending->electionVotes.end() || !it->second)
        {
            continue;
        }
        tallies[*it->second] += weight;
    }

    // tallies is keyed in ascending faction id, so a strict > keeps the lowest-id candidate
    // on a tie — the intended tie-break.
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

    bool passed = true;
    if (rConfig.voteThreshold > 0.0)
    {
        const double share =
            totalWeight > 0 ? static_cast<double>(bestVotes) / totalWeight : 0.0;
        passed = share >= rConfig.voteThreshold;
    }
    return passed ? FindMember_(bestId) : nullptr;
}

ResolveProposalResult_t PlanetaryCouncil::ApplyPassedProposal_(
    GameState& rGameState, const CouncilProposalConfig_t& rConfig, Faction* pElectionWinner,
    bool overruled)
{
    for (const std::string& rRepealId : rConfig.repeals)
    {
        RemoveActiveProposal_(rRepealId);
    }

    ++m_passCounts[rConfig.id];
    m_applier.ApplyInstantaneousEffects(m_members, rConfig);

    if (rConfig.electionOutcome != CouncilElectionOutcome_t::PlanetaryGovernor
        && rConfig.electionOutcome != CouncilElectionOutcome_t::SupremeLeaderVictory)
    {
        ActivateProposal_(rConfig);
    }

    ResolveProposalResult_t result =
        overruled ? ResolveProposalResult_t::VetoOverruled : ResolveProposalResult_t::Passed;

    if (rConfig.electionOutcome == CouncilElectionOutcome_t::PlanetaryGovernor
        && pElectionWinner)
    {
        ApplyGovernor_(rGameState, *pElectionWinner);
        ActivateProposal_(rConfig);
    }
    else if (rConfig.electionOutcome == CouncilElectionOutcome_t::SupremeLeaderVictory
             && pElectionWinner)
    {
        m_pSupremeLeaderVictory = pElectionWinner;
        result = ResolveProposalResult_t::SupremeLeaderVictoryStub;
    }

    return result;
}

ResolveProposalResult_t PlanetaryCouncil::ResolveOutcome_(GameState& rGameState)
{
    if (m_pending->vetoed && !VetoUnanimouslyOverruled_())
    {
        return ResolveProposalResult_t::Vetoed;
    }
    const bool overruled = m_pending->vetoed && VetoUnanimouslyOverruled_();

    const CouncilProposalConfig_t& rConfig = m_rRegistry.Get(m_pending->proposalId);

    Faction* pElectionWinner = nullptr;
    bool passed = false;
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        pElectionWinner = TallyElection_(rConfig);
        passed = pElectionWinner != nullptr;
    }
    else
    {
        passed = TallyStandard_(rConfig);
    }

    if (!passed)
    {
        return ResolveProposalResult_t::Failed;
    }
    return ApplyPassedProposal_(rGameState, rConfig, pElectionWinner, overruled);
}

ResolveProposalResult_t PlanetaryCouncil::Resolve(GameState& rGameState)
{
    if (!m_pending)
    {
        throw std::invalid_argument("PlanetaryCouncil::Resolve: no pending proposal");
    }
    if (!AllMembersVoted())
    {
        throw std::invalid_argument("PlanetaryCouncil::Resolve: votes incomplete");
    }

    const std::string proposalId = m_pending->proposalId;
    const ResolveProposalResult_t result = ResolveOutcome_(rGameState);

    m_pending.reset();
    m_revision.Bump();
    OnResolved.Emit(proposalId, result);
    return result;
}

std::vector<ActiveEffect_t> PlanetaryCouncil::CollectWorldEffects() const
{
    return m_effects.WorldEffects();
}

std::vector<ActiveEffect_t> PlanetaryCouncil::CollectFactionEffects(const Faction& rFaction) const
{
    if (m_pGovernor != &rFaction)
    {
        return {};
    }
    return m_effects.GovernorEffects();
}

} // namespace ac
