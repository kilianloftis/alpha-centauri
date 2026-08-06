#pragma once

#include "game/council/CouncilEffects.h"
#include "game/council/CouncilOutcomeApplier.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/base/BaseTypes.h"
#include "lib/Revision.h"
#include "lib/Signal.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class CouncilProposalRegistry;
class DiplomacyLedger;
class Faction;
class GameState;

enum class CouncilBallot_t
{
    Yea,
    Nay,
    Abstain,
};

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

// Runtime Planetary Council: proposals, voting, governor, and active effects.
// Calling Propose starts a single vote sequence (cooldown + commlink share + pending
// ballots); there is no separate convene/session stage.
// Member factions are fixed at construction (non-owning); they must outlive the council.
class PlanetaryCouncil
{
public:
    PlanetaryCouncil(const CouncilProposalRegistry& rRegistry,
                     const CouncilRulesConfig_t& rRules,
                     std::vector<Faction*> members);

    const CouncilProposalRegistry& GetRegistry() const { return m_rRegistry; }

    // --- Membership (fixed at construction) ---
    const std::vector<Faction*>& Members() const { return m_members; }
    bool IsCouncilMember(const Faction& rFaction) const;

    bool HasCommlinksToAllMembers(const GameState& rGameState, const Faction& rFaction) const;
    int ProposeCooldownYears(const Faction& rFaction) const;
    int YearsUntilCanPropose(const GameState& rGameState, const Faction& rFaction) const;
    // Mission year of this faction's last Propose, or nullopt if they have never proposed.
    std::optional<int> LastProposedYear(const Faction& rFaction) const;
    const CouncilRulesConfig_t& GetRules() const { return m_rRules; }

    // --- Proposal availability ---
    // In force: this proposal contributes continuous world effects right now. Distinct from
    // "has passed" (see the pass-count history) — a one-shot or a pure repeal is never in force.
    bool IsActive(const std::string& rProposalId) const;
    // Enacted at least once, whether or not it is still in force.
    bool HasPassed(const std::string& rProposalId) const;
    // True when any active council WorldGlobal effect carries the given RuleFlag.
    bool HasActiveRuleFlag(RuleFlagId_t flag) const;

    // Proposal eligibility (tech, active/repeal gates). Does not check cooldown or
    // commlinks — those are validated when Propose is called.
    bool CanPropose(const Faction& rProposer, const std::string& rProposalId) const;

    // Put an issue before the council: checks membership, commlinks, cooldown, and
    // availability; shares commlinks among members; starts the proposer's cooldown;
    // opens a pending vote. Other members cast votes, then Resolve.
    void Propose(GameState& rGameState, Faction& rProposer, const std::string& rProposalId);

    // --- Voting (standard) ---
    void CastVote(const Faction& rVoter, CouncilBallot_t ballot);
    // Election: pCandidate null = abstain.
    void CastElectionVote(const Faction& rVoter, const Faction* pCandidate);

    // Governor veto. Returns false if there is no pending proposal or caller is not governor.
    // Overruled at Resolve when every non-governor member voted Yea.
    bool VetoPending(const Faction& rGovernor);

    // True when every council member has a ballot (or election vote) on the pending proposal.
    bool AllMembersVoted() const;

    ResolveProposalResult_t Resolve(GameState& rGameState);

    // --- Governor / victory stub ---
    Faction* GetPlanetaryGovernor() const { return m_pGovernor; }
    Faction* GetSupremeLeaderVictoryStub() const { return m_pSupremeLeaderVictory; }

    // Top two council members by total population (ties broken by lower faction id).
    std::vector<Faction*> GovernorCandidates() const;

    // Members that may be voted for in this proposal's election: GovernorCandidates() for a
    // Planetary Governor election, every member for Supreme Leader. The single source of truth
    // for the rule — CastElectionVote enforces it, and the UI should offer exactly this list.
    std::vector<Faction*> EligibleCandidates(const CouncilProposalConfig_t& rConfig) const;

    // Vote weight for a member under the given mode (applies CouncilVotes effects).
    int ComputeVoteWeight(const Faction& rFaction, CouncilVoteWeight_t mode) const;

    // Continuous WorldGlobal effects from active proposals (Trade Pact, U.N. Charter, etc.).
    // Wrappers' `config` pointers reference the proposal registry, so a retained wrapper
    // stays valid across any rebuild. The returned vector itself is rebuilt in place by
    // RebuildWorld — copy the wrappers out rather than holding this reference across a vote.
    const std::vector<ActiveEffect_t>& CollectWorldEffects() const;
    // Continuous FactionGlobal effects for the governor (commerce energy bonus).
    // Empty when rFaction is not the governor.
    const std::vector<ActiveEffect_t>& CollectFactionEffects(const Faction& rFaction) const;

    // Bumped on every RebuildWorld / SetGovernorEffects so Faction composed-pool caches
    // recompose. The borrowed config addresses themselves are stable across rebuilds.
    const Revision& GetRevision() const { return m_revision; }

    struct PendingProposal_t
    {
        std::string proposalId;
        FactionId_t proposerId = 0;
        bool vetoed = false;
        // Standard ballots; election votes use electionVotes instead.
        std::map<FactionId_t, CouncilBallot_t> ballots;
        std::map<FactionId_t, std::optional<FactionId_t>> electionVotes;
        // Who may be voted for, fixed when the vote opened (empty for a standard ballot).
        // Snapshotted because eligibility is derived from live population — see Propose.
        std::vector<FactionId_t> eligibleCandidateIds;
    };

    const PendingProposal_t* GetPending() const
    {
        return m_pending ? &*m_pending : nullptr;
    }

    // --- Notifications (for UI / AI observers) ---
    // Fired when a proposal is put before the council (proposer, proposalId): the UI opens
    // the vote popup and AI observers may decide their ballots. Read GetPending() for detail.
    Signal<Faction&, const std::string&> OnProposalOpened;
    // Fired once a pending proposal resolves, for every outcome (proposalId, result).
    Signal<const std::string&, ResolveProposalResult_t> OnResolved;

private:
    void ActivateInitiallyActiveProposals_();
    void RemoveActiveProposal_(const std::string& rProposalId);
    void ActivateProposal_(const CouncilProposalConfig_t& rConfig);
    void ApplyGovernor_(GameState& rGameState, Faction& rGovernor);
    bool VetoUnanimouslyOverruled_() const;
    Faction* FindMember_(FactionId_t factionId) const;
    // Against m_pending->eligibleCandidateIds; requires a pending election.
    bool IsEligibleCandidate_(const Faction& rCandidate) const;
    bool HasPassed_(const std::string& rProposalId) const;

    // --- Resolve helpers ---
    // Decide the outcome of the pending proposal (veto, tally, apply). Leaves m_pending in
    // place; the caller clears it and fires OnResolved.
    ResolveProposalResult_t ResolveOutcome_(GameState& rGameState);
    // Tally a standard ballot. With voteThreshold == 0 this is "weighted Yea outvotes Nay";
    // with a threshold it is "weighted Yea reaches that share of *total* member weight", so an
    // abstention counts against reaching the bar rather than being neutral. Same rule, same
    // denominator, as TallyElection_.
    bool TallyStandard_(const CouncilProposalConfig_t& rConfig) const;
    // Tally an election; returns the winning member, or null when no candidate clears the
    // threshold (i.e. the proposal failed).
    Faction* TallyElection_(const CouncilProposalConfig_t& rConfig) const;
    // Apply a passed proposal (repeals, instantaneous effects, activation, governor /
    // victory outcomes) and return the result. pElectionWinner is null for standard ballots.
    ResolveProposalResult_t ApplyPassedProposal_(GameState& rGameState,
                                                 const CouncilProposalConfig_t& rConfig,
                                                 Faction* pElectionWinner,
                                                 bool overruled);

    const CouncilProposalRegistry& m_rRegistry;
    const CouncilRulesConfig_t& m_rRules;

    // Non-owning; fixed for the life of the council. Members must outlive this object.
    std::vector<Faction*> m_members;

    // Mission year when each faction last put a proposal before the council.
    std::map<FactionId_t, int> m_lastProposedYear;

    Faction* m_pGovernor = nullptr;
    Faction* m_pSupremeLeaderVictory = nullptr;

    // Active continuous proposal ids (in force).
    std::vector<std::string> m_activeProposalIds;
    // Lifetime pass counts (instantaneous one-shots / history).
    std::map<std::string, int> m_passCounts;

    // Continuous world effects in force, plus governor-only faction effects.
    CouncilEffects m_effects;
    // Applies passed-proposal game mutations (energy grants, sea level, governor privileges).
    CouncilOutcomeApplier m_applier;

    std::optional<PendingProposal_t> m_pending;
    Revision m_revision;
};

} // namespace ac
