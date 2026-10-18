#include "game/council/PlanetaryCouncil.h"

#include "game/council/CouncilProposalRegistry.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/Faction.h"

#include <algorithm>
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
    for (const CouncilProposalConfig_t& rConfig : m_rRegistry.GetAll())
    {
        if (rConfig.initiallyActive)
        {
            Enact(rConfig);
        }
    }
}

bool PlanetaryCouncil::IsCouncilMember(const Faction& rFaction) const
{
    return std::find(m_members.begin(), m_members.end(), &rFaction) != m_members.end();
}

void PlanetaryCouncil::Expel(const Faction& rFaction)
{
    const auto it = std::find(m_members.begin(), m_members.end(), &rFaction);
    if (it == m_members.end())
    {
        return;
    }
    m_members.erase(it);
    if (m_pGovernor == &rFaction)
    {
        m_pGovernor = nullptr;
        m_effects.ClearGovernorEffects();
    }
    m_revision.Bump();
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
    const auto it = m_passCounts.find(rProposalId);
    return it != m_passCounts.end() && it->second > 0;
}

bool PlanetaryCouncil::HasActiveRuleFlag(RuleFlagId_t flag) const
{
    return m_effects.HasActiveRuleFlag(flag);
}

void PlanetaryCouncil::RecordProposal(FactionId_t proposer, int missionYear)
{
    m_lastProposedYear[proposer] = missionYear;
    m_revision.Bump();
}

void PlanetaryCouncil::RecordPassed(const CouncilProposalConfig_t& rConfig)
{
    for (const std::string& rRepealId : rConfig.repeals)
    {
        RemoveActiveProposal_(rRepealId);
    }
    ++m_passCounts[rConfig.id];
}

void PlanetaryCouncil::RemoveActiveProposal_(const std::string& rProposalId)
{
    std::erase(m_activeProposalIds, rProposalId);
    m_effects.RebuildWorld(m_activeProposalIds, m_rRegistry);
    m_revision.Bump();
}

void PlanetaryCouncil::Enact(const CouncilProposalConfig_t& rConfig)
{
    // The in-force set means "this law contributes continuous world effects now"; a pure
    // repeal in it would let CanPropose's non-repeatable rule consume it forever.
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

void PlanetaryCouncil::InstallGovernor(Faction& rGovernor)
{
    m_pGovernor = &rGovernor;
    m_effects.SetGovernorEffects(m_rRules.governorEffects);
    m_revision.Bump();
}

void PlanetaryCouncil::SetSupremeLeaderVictory(Faction& rWinner)
{
    m_pSupremeLeaderVictory = &rWinner;
}

const std::vector<ActiveEffect_t>& PlanetaryCouncil::CollectWorldEffects() const
{
    return m_effects.WorldEffects();
}

const std::vector<ActiveEffect_t>& PlanetaryCouncil::CollectFactionEffects(
    const Faction& rFaction) const
{
    static const std::vector<ActiveEffect_t> k_Empty;
    if (m_pGovernor != &rFaction)
    {
        return k_Empty;
    }
    return m_effects.GovernorEffects();
}

} // namespace ac
