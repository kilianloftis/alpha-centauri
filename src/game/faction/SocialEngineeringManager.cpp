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
    std::vector<ActiveEffect_t> result;
    std::map<SocialRatingId, int> ratingTotals;

    for (const auto& [category, id] : m_activePolicyIds)
    {
        const SocialPolicyConfig* pPolicy = m_pRegistry ? m_pRegistry->Find(id) : nullptr;
        if (!pPolicy)
        {
            continue;
        }

        for (const auto& rEffect : pPolicy->effects)
        {
            if (const auto* pRatingMod = std::get_if<SocialRatingModifierEffect_t>(&rEffect.effect))
            {
                ratingTotals[pRatingMod->rating] += pRatingMod->amount;
            }
            else
            {
                result.push_back({ &rEffect, id, nullptr });
            }
        }
    }

    if (!m_pRatingRegistry)
    {
        return result;
    }

    for (const auto& [rating, total] : ratingTotals)
    {
        if (total == 0)
        {
            continue;
        }

        const SocialRatingConfig* pRatingConfig = m_pRatingRegistry->Find(
            SocialRatingIdToString(rating));
        if (!pRatingConfig)
        {
            continue;
        }

        const auto it = pRatingConfig->levelEffects.find(total);
        if (it == pRatingConfig->levelEffects.end())
        {
            continue;
        }

        const std::string sourceId = "se_rating_" + SocialRatingIdToString(rating)
                                     + "_" + std::to_string(total);
        for (const auto& rEffect : it->second)
        {
            result.push_back({ &rEffect, sourceId, nullptr });
        }
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
