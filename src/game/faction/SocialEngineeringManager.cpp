#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include <map>
#include <stdexcept>

namespace ac
{

namespace
{

constexpr const char* k_DefaultPoliticsPolicyId      = "frontier";
constexpr const char* k_DefaultEconomicsPolicyId     = "simple";
constexpr const char* k_DefaultValuesPolicyId        = "survival";
constexpr const char* k_DefaultFutureSocietyPolicyId = "none_future";

} // namespace

SocialEngineeringManager::SocialEngineeringManager(const SocialPolicyRegistry& rRegistry)
    : m_rRegistry(rRegistry)
{
    // Fail fast if the config doesn't provide these hardcoded defaults: without this check
    // a missing/miscategorized default silently leaves GetActivePolicy() returning nullptr
    // for that category forever, instead of failing at faction construction.
    const std::map<SocialCategory_t, const char*> defaults = {
        { SocialCategory_t::Politics,      k_DefaultPoliticsPolicyId },
        { SocialCategory_t::Economics,     k_DefaultEconomicsPolicyId },
        { SocialCategory_t::Values,        k_DefaultValuesPolicyId },
        { SocialCategory_t::FutureSociety, k_DefaultFutureSocietyPolicyId }
    };

    for (const auto& [category, id] : defaults)
    {
        const SocialPolicyConfig_t& rPolicy = m_rRegistry.Get(id); // throws if unknown
        if (rPolicy.category != category)
        {
            throw std::runtime_error(
                "SocialEngineeringManager: default policy '" + std::string(id)
                + "' is not in its expected category");
        }
        m_activePolicies[category] = &rPolicy;
    }
}

SocialEngineeringManager::~SocialEngineeringManager()
{
}

void SocialEngineeringManager::SetActivePolicy(const SocialPolicyConfig_t& rPolicy)
{
    // Store the registry instance so the pointer outlives the caller's temporary.
    const SocialPolicyConfig_t& rCanonical = m_rRegistry.Get(rPolicy.id);
    m_activePolicies[rCanonical.category] = &rCanonical;
    m_revision.Bump();
}

const SocialPolicyConfig_t* SocialEngineeringManager::GetActivePolicy(SocialCategory_t category) const
{
    const auto it = m_activePolicies.find(category);
    if (it == m_activePolicies.end())
    {
        return nullptr;
    }
    return it->second;
}

std::vector<ActiveEffect_t> SocialEngineeringManager::CollectEffects() const
{
    // Policy effects pass through as ordinary active effects — including SocialRatingModifier
    // entries. Rating accumulation and the rating-level -> gameplay-effects mapping happen
    // later, per base, in ResolveSocialRatingLevelEffects (SocialRatingResolver): modifiers
    // can come from any source (buildings, pops, ...), and ThisBase-scoped ones shift a
    // single base's effective rating.
    std::vector<ActiveEffect_t> result;
    for (const auto& [category, pPolicy] : m_activePolicies)
    {
        if (!pPolicy)
        {
            continue;
        }
        AppendActiveEffects(pPolicy->effects, nullptr, pPolicy->id, result);
    }
    return result;
}

int SocialEngineeringManager::GetSocialRating(SocialRatingId_t rating) const
{
    const std::map<SocialRatingId_t, int> totals = AccumulateSocialRatings(CollectEffects());
    const auto it = totals.find(rating);
    return it == totals.end() ? 0 : it->second;
}

std::vector<const SocialPolicyConfig_t*> SocialEngineeringManager::GetAvailablePolicies(
    SocialCategory_t category,
    const std::vector<std::string>& rDiscoveredTechIds) const
{
    std::vector<const SocialPolicyConfig_t*> result;
    for (const SocialPolicyConfig_t* pConfig : m_rRegistry.GetByCategory(category))
    {
        if (pConfig->IsAvailable(rDiscoveredTechIds))
        {
            result.push_back(pConfig);
        }
    }
    return result;
}

} // namespace ac
