#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include <map>
#include <stdexcept>

namespace ac
{

SocialEngineeringManager::SocialEngineeringManager(const SocialPolicyRegistry* pRegistry,
                                                   const SocialRatingRegistry* pRatingRegistry)
    : m_pRegistry(pRegistry)
    , m_pRatingRegistry(pRatingRegistry)
    , m_activePolicyIds({
        { SocialCategory_t::Politics,      "frontier"     },
        { SocialCategory_t::Economics,     "simple"       },
        { SocialCategory_t::Values,        "survival"     },
        { SocialCategory_t::FutureSociety, "none_future"  }
    })
{
    // Fail fast if the config doesn't provide these hardcoded defaults: without this check
    // a missing/miscategorized default silently leaves GetActivePolicy() returning nullptr
    // for that category forever, instead of failing at faction construction.
    if (m_pRegistry)
    {
        for (const auto& [category, id] : m_activePolicyIds)
        {
            const SocialPolicyConfig_t& rPolicy = m_pRegistry->Get(id); // throws if unknown
            if (rPolicy.category != category)
            {
                throw std::runtime_error(
                    "SocialEngineeringManager: default policy '" + id
                    + "' is not in its expected category");
            }
        }
    }
}

SocialEngineeringManager::~SocialEngineeringManager()
{
}

void SocialEngineeringManager::SetActivePolicy(SocialCategory_t category, const std::string& policyId)
{
    if (!m_pRegistry)
    {
        throw std::runtime_error("SocialEngineeringManager::SetActivePolicy: policy registry is null");
    }
    const SocialPolicyConfig_t& rPolicy = m_pRegistry->Get(policyId); // throws if unknown
    if (rPolicy.category != category)
    {
        throw std::runtime_error(
            "SocialEngineeringManager::SetActivePolicy: policy '" + policyId
            + "' does not belong to the requested category");
    }
    m_activePolicyIds[category] = policyId;
    m_revision.Bump();
}

const SocialPolicyConfig_t* SocialEngineeringManager::GetActivePolicy(SocialCategory_t category) const
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
        const SocialPolicyConfig_t* pPolicy = m_pRegistry ? m_pRegistry->Find(id) : nullptr;
        if (!pPolicy)
        {
            continue;
        }
        AppendActiveEffects(pPolicy->effects, nullptr, id, result);
    }
    return result;
}

std::vector<const SocialPolicyConfig_t*> SocialEngineeringManager::GetAvailablePolicies(
    SocialCategory_t category,
    const std::vector<std::string>& rDiscoveredTechIds) const
{
    if (!m_pRegistry)
    {
        return {};
    }

    std::vector<const SocialPolicyConfig_t*> result;
    for (const SocialPolicyConfig_t* pConfig : m_pRegistry->GetByCategory(category))
    {
        if (pConfig->IsAvailable(rDiscoveredTechIds))
        {
            result.push_back(pConfig);
        }
    }
    return result;
}

} // namespace ac
