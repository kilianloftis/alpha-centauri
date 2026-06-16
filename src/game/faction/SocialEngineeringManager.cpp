#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"

namespace ac
{

SocialEngineeringManager::SocialEngineeringManager(const SocialPolicyRegistry* pRegistry)
    : m_pRegistry(pRegistry)
    , m_activePolicyIds({
        { SocialCategory::Politics,      "frontier"     },
        { SocialCategory::Economics,     "simple"       },
        { SocialCategory::Values,        "survival"     },
        { SocialCategory::FutureSociety, "none_future"  }
    })
{
}

SocialEngineeringManager::~SocialEngineeringManager()
{
}

bool SocialEngineeringManager::SetActivePolicy(SocialCategory category, const std::string& policyId)
{
    if (!m_pRegistry || !m_pRegistry->Find(policyId))
    {
        return false;
    }
    m_activePolicyIds[category] = policyId;
    return true;
}

const SocialPolicyConfig* SocialEngineeringManager::GetActivePolicy(SocialCategory category) const
{
    auto it = m_activePolicyIds.find(category);
    if (it == m_activePolicyIds.end() || !m_pRegistry)
    {
        return nullptr;
    }
    return m_pRegistry->Find(it->second);
}

SocialScores SocialEngineeringManager::GetCombinedScores() const
{
    SocialScores combined;
    for (const auto& [category, id] : m_activePolicyIds)
    {
        const SocialPolicyConfig* pPolicy = m_pRegistry ? m_pRegistry->Find(id) : nullptr;
        if (!pPolicy)
        {
            continue;
        }
        const SocialScores& rFx = pPolicy->effects;
        combined.economy    += rFx.economy;
        combined.efficiency += rFx.efficiency;
        combined.support    += rFx.support;
        combined.police     += rFx.police;
        combined.morale     += rFx.morale;
        combined.growth     += rFx.growth;
        combined.planet     += rFx.planet;
        combined.research   += rFx.research;
        combined.industry   += rFx.industry;
        combined.probe      += rFx.probe;
    }
    return combined;
}

std::vector<const SocialPolicyConfig*> SocialEngineeringManager::GetAvailablePolicies(
    SocialCategory category,
    const std::vector<std::string>& rDiscoveredTechIds) const
{
    if (!m_pRegistry)
    {
        return {};
    }

    std::vector<const SocialPolicyConfig*> result;
    for (const SocialPolicyConfig* pConfig : m_pRegistry->GetByCategory(category))
    {
        if (pConfig->IsAvailable(rDiscoveredTechIds))
        {
            result.push_back(pConfig);
        }
    }
    return result;
}

} // namespace ac
