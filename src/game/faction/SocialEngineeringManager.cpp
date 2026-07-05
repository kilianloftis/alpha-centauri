#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include <map>

namespace ac
{

SocialEngineeringManager::SocialEngineeringManager(const SocialPolicyRegistry* pRegistry,
                                                   const SocialRatingRegistry* pRatingRegistry)
    : m_pRegistry(pRegistry)
    , m_pRatingRegistry(pRatingRegistry)
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

std::vector<ActiveEffect_t> SocialEngineeringManager::CollectEffects() const
{
    // Policy effects pass through as ordinary active effects — including SocialRatingModifier
    // entries. Rating accumulation and the rating-level -> gameplay-effects mapping happen
    // later, per base, in ExpandSocialRatingEffects (SocialRatingResolver): rating modifiers
    // can come from any source (buildings, pops, ...), and ThisBase-scoped ones shift a
    // single base's effective rating.
    std::vector<ActiveEffect_t> result;
    for (const auto& [category, id] : m_activePolicyIds)
    {
        const SocialPolicyConfig* pPolicy = m_pRegistry ? m_pRegistry->Find(id) : nullptr;
        if (!pPolicy)
        {
            continue;
        }
        AppendActiveEffects(pPolicy->effects, nullptr, id, result);
    }
    return result;
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
