#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/social-engineering/SocialRatingResolver.h"
#include <magic_enum.hpp>
#include <map>
#include <stdexcept>

namespace ac
{

SocialEngineeringManager::SocialEngineeringManager(const SocialPolicyRegistry& rRegistry)
    : m_rRegistry(rRegistry)
{
    // Starting policies come from config (`"default": true`), not from ids compiled in here. A
    // mod shipping its own policy set used to hard-fail every faction constructor.
    for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
    {
        m_activePolicies[category] = &m_rRegistry.GetDefaultForCategory(category);
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

const std::map<SocialRatingId_t, int>& SocialEngineeringManager::AccumulatedRatings_() const
{
    const uint64_t revision = m_revision.Get();
    if (m_cachedRatingsRevision != revision)
    {
        m_cachedRatings = AccumulateSocialRatings(CollectEffects());
        m_cachedRatingsRevision = revision;
    }
    return m_cachedRatings;
}

int SocialEngineeringManager::GetSocialRating(SocialRatingId_t rating) const
{
    const std::map<SocialRatingId_t, int>& rTotals = AccumulatedRatings_();
    const auto it = rTotals.find(rating);
    return it == rTotals.end() ? 0 : it->second;
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
