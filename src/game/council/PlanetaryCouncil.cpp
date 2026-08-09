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

bool PlanetaryCouncil::HasPassed(const std::string& rProposalId) const
{
    return HasPassed_(rProposalId);
}

bool PlanetaryCouncil::HasPassed_(const std::string& rProposalId) const
{
    const auto it = m_passCounts.find(rProposalId);
    return it != m_passCounts.end() && it->second > 0;
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

std::vector<Faction*> PlanetaryCouncil::EligibleCandidates(
    const CouncilProposalConfig_t& rConfig) const
{
    if (rConfig.electionOutcome == CouncilElectionOutcome_t::PlanetaryGovernor)
    {
        return GovernorCandidates();
    }
    // Supreme Leader is open to the whole council.
    return m_members;
}

bool PlanetaryCouncil::IsEligibleCandidate_(const Faction& rCandidate) const
{
    // Against the snapshot taken when the vote opened, not a fresh computation — see Propose.
    const std::vector<FactionId_t>& rEligible = m_pending->eligibleCandidateIds;
    return std::find(rEligible.begin(), rEligible.end(), rCandidate.GetFactionId())
           != rEligible.end();
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

    // required_proposals asks "has this been enacted", not "is it still in force". The two
    // used to be the same set, which is why splitting them matters here: launch_solar_shade
    // carries only an Instantaneous effect, so it is history rather than a standing law, yet
    // increase_solar_shade must still require it.
    for (const std::string& rRequired : pConfig->requiredProposals)
    {
        if (!HasPassed_(rRequired) && !IsActive(rRequired))
        {
            return false;
        }
    }

    // A repeal is meaningful only while something it repeals is actually in force. This is the
    // "is it still standing" half of the split above, and it is what keeps a repeal proposable
    // again after its target is re-enacted (see the consumption rule below).
    if (!pConfig->repeals.empty())
    {
        const bool bAnyTargetInForce =
            std::any_of(pConfig->repeals.begin(), pConfig->repeals.end(),
                        [this](const std::string& rId) { return IsActive(rId); });
        if (!bAnyTargetInForce)
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
    //
    // Repeals are exempt for the same reason. A proposal that repeals something is not a
    // one-shot: it becomes meaningful again whenever its target is back in force, and its own
    // availability already tracks that through config gates (repeal_trade_pact declares
    // required_proposals: [global_trade_pact]; repeal_un_charter declares
    // requires_rule_flags: [atrocities_forbidden], both checked above). Consuming it after one
    // use is what made the charter unrepealable once reinstated.
    if (!pConfig->repeatable && !IsActive(rProposalId) && !HasContinuousWorldEffects_(*pConfig)
        && pConfig->repeals.empty())
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
    // Snapshot who may be voted for. The eligible set is derived from live population, so
    // recomputing it per ballot would let a faction drop out of the top two mid-vote: ballots
    // already cast for it would silently stand while later voters could not choose it, and
    // the tally would elect a faction that is no longer eligible. One set, fixed for the vote.
    const CouncilProposalConfig_t& rConfig = m_rRegistry.Get(rProposalId);
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        for (const Faction* pCandidate : EligibleCandidates(rConfig))
        {
            pending.eligibleCandidateIds.push_back(pCandidate->GetFactionId());
        }
    }
    m_pending = std::move(pending);
    m_lastProposedYear[rProposer.GetFactionId()] = rGameState.GetMissionYear();
    m_revision.Bump();

    // Observers (the AI voter, the UI) run here. If one throws, the pending slot must not
    // survive: Propose throws while m_pending is set and only Resolve clears it, so a stranded
    // pending proposal is the terminal state this package exists to remove.
    try
    {
        OnProposalOpened.Emit(rProposer, rProposalId);
    }
    catch (...)
    {
        m_pending.reset();
        m_revision.Bump();
        throw;
    }
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
    // Eligibility is the council's rule, enforced where the vote is cast. It used to live only
    // in GovernorCandidates(), which nothing enforcing anything called — the UI passed the full
    // membership as the candidate list, so any member could be elected Planetary Governor
    // despite the "two most populous factions" rule stated in the proposal description and in
    // docs/architecture/council-system.md.
    if (pCandidate && !IsEligibleCandidate_(*pCandidate))
    {
        throw std::invalid_argument(
            "PlanetaryCouncil::CastElectionVote: '" + pCandidate->GetDefinitionId()
            + "' is not an eligible candidate for '" + rConfig.id + "'");
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
    // Governor rules do not apply to elections (docs/game-rules-decisions.md #5): the office
    // cannot be defended by its incumbent. This is why VetoUnanimouslyOverruled_ may read
    // standard ballots — an election never reaches it.
    if (m_rRegistry.Get(m_pending->proposalId).kind == CouncilProposalKind_t::Election)
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
    // A veto stands unless every council member other than the governor voted Yea. Standard
    // ballots only, which is correct because an election cannot be vetoed (see VetoPending).
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
    // The in-force set means "this law currently contributes continuous world effects" — not
    // "this proposal has passed at some point", which is what m_passCounts records. Marking a
    // pure repeal (no effects of its own) as active made CanPropose's `IsActive && !repeatable`
    // rule consume it forever, so a repealed-then-reinstated law could never be repealed again.
    if (!HasContinuousWorldEffects_(rConfig))
    {
        return;
    }
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
    int totalWeight = 0;
    for (Faction* pMember : m_members)
    {
        const int weight = ComputeVoteWeight(*pMember, rConfig.voteWeight);
        totalWeight += weight;
        const auto it = m_pending->ballots.find(pMember->GetFactionId());
        // A missing ballot is an abstention (see Resolve): it counts toward the total weight
        // a threshold is measured against, but for neither side.
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

    // voteThreshold is documented as "fraction of total vote weight required for passage;
    // 0 = simple majority". It was read by TallyElection_ only, so a modder setting it on a
    // standard proposal silently got a plain majority.
    if (rConfig.voteThreshold > 0.0)
    {
        const double share = totalWeight > 0 ? static_cast<double>(yea) / totalWeight : 0.0;
        return share >= rConfig.voteThreshold;
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
        // Re-check eligibility at tally time as well as at cast time. Cast-time validation
        // alone is not enough: a ballot is only as good as the moment it was cast, and this
        // is the last point before a winner is installed as governor.
        const Faction* pCandidate = FindMember_(*it->second);
        if (!pCandidate || !IsEligibleCandidate_(*pCandidate))
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

    // Unconditional: ActivateProposal_ itself decides what belongs in the in-force set (only
    // proposals carrying continuous world effects), so the election special-cases that used to
    // guard this call are redundant.
    ActivateProposal_(rConfig);

    ResolveProposalResult_t result =
        overruled ? ResolveProposalResult_t::VetoOverruled : ResolveProposalResult_t::Passed;

    if (rConfig.electionOutcome == CouncilElectionOutcome_t::PlanetaryGovernor
        && pElectionWinner)
    {
        ApplyGovernor_(rGameState, *pElectionWinner);
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
    // Deliberately no "all members voted" precondition. A member that never casts a ballot
    // abstains — which is exactly what both tallies already do with a missing entry. Requiring
    // unanimous participation made the pending slot a terminal state: nothing else clears
    // m_pending and Propose throws while it is set, so one silent member bricked the council
    // for the rest of the game. AllMembersVoted() remains available for a UI that wants to
    // wait before offering to resolve.

    const std::string proposalId = m_pending->proposalId;
    const ResolveProposalResult_t result = ResolveOutcome_(rGameState);

    m_pending.reset();
    m_revision.Bump();
    OnResolved.Emit(proposalId, result);
    return result;
}

const std::vector<ActiveEffect_t>& PlanetaryCouncil::CollectWorldEffects() const
{
    return m_effects.WorldEffects();
}

const std::vector<ActiveEffect_t>& PlanetaryCouncil::CollectFactionEffects(const Faction& rFaction) const
{
    static const std::vector<ActiveEffect_t> k_Empty;
    if (m_pGovernor != &rFaction)
    {
        return k_Empty;
    }
    return m_effects.GovernorEffects();
}

} // namespace ac
