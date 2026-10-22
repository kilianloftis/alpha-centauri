#pragma once

#include "game/council/CouncilEffects.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/base/BaseTypes.h"
#include "lib/Revision.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class CouncilProposalRegistry;
class Faction;

// The council's standing state: members (seated at construction, outliving it; Expel is the one
// way out), the governor, and the laws in force with their history.
class PlanetaryCouncil
{
public:
    PlanetaryCouncil(const CouncilProposalRegistry& rRegistry,
                     const CouncilRulesConfig_t& rRules,
                     std::vector<Faction*> members);

    const CouncilProposalRegistry& GetRegistry() const { return m_rRegistry; }
    const CouncilRulesConfig_t& GetRules() const { return m_rRules; }

    // --- Membership ---
    const std::vector<Faction*>& Members() const { return m_members; }
    bool IsCouncilMember(const Faction& rFaction) const;

    // Remove a member for good, vacating the governorship and its standing effects if they held
    // it. Expelling a faction that is not a member does nothing.
    void Expel(const Faction& rFaction);

    // Mission year of this faction's last proposal, or nullopt if they have never proposed.
    std::optional<int> LastProposedYear(const Faction& rFaction) const;

    // --- Laws ---
    // In force now; a one-shot or a pure repeal never is, though it HasPassed.
    bool IsActive(const std::string& rProposalId) const;
    // Enacted at least once, whether or not it is still in force.
    bool HasPassed(const std::string& rProposalId) const;
    // True when any active council WorldGlobal effect carries the given RuleFlag.
    bool HasActiveRuleFlag(RuleFlagId_t flag) const;

    // --- Governor / victory stub ---
    Faction* GetPlanetaryGovernor() const { return m_pGovernor; }
    Faction* GetSupremeLeaderVictoryStub() const { return m_pSupremeLeaderVictory; }

    // --- Transitions CouncilVoteExecutor makes ---
    void RecordProposal(FactionId_t proposer, int missionYear);
    // Lifts the laws a passed proposal repeals and counts the pass.
    void RecordPassed(const CouncilProposalConfig_t& rConfig);
    // Puts a passed proposal's continuous world effects in force; one without them stays out.
    void Enact(const CouncilProposalConfig_t& rConfig);
    void InstallGovernor(Faction& rGovernor);
    void SetSupremeLeaderVictory(Faction& rWinner);

    // Laws' WorldGlobal effects. The configs are stable, but the vector rebuilds in place, so copy
    // it rather than hold it across a vote.
    const std::vector<ActiveEffect_t>& CollectWorldEffects() const;
    // Continuous FactionGlobal effects for the governor (commerce energy bonus).
    // Empty when rFaction is not the governor.
    const std::vector<ActiveEffect_t>& CollectFactionEffects(const Faction& rFaction) const;

    // Bumped whenever membership, the governor or the laws in force change, so Faction
    // composed-pool caches recompose. The borrowed config addresses are stable across rebuilds.
    const Revision& GetRevision() const { return m_revision; }

private:
    void RemoveActiveProposal_(const std::string& rProposalId);

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
    // Lifetime pass counts (one-shot outcomes / history).
    std::map<std::string, int> m_passCounts;

    // Continuous world effects in force, plus governor-only faction effects.
    CouncilEffects m_effects;
    Revision m_revision;
};

} // namespace ac
