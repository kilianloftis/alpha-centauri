#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyDraft.h"
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
    // Starting policies come from config (`"default": true`), so a mod can ship its own set.
    for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
    {
        const SocialPolicyConfig_t* pDefault = &m_rRegistry.GetDefaultForCategory(category);
        m_pendingPolicies[category] = pDefault;
        m_committedPolicies[category] = pDefault;
    }
}

SocialEngineeringManager::~SocialEngineeringManager()
{
}

void SocialEngineeringManager::SetActivePolicy(const SocialPolicyConfig_t& rPolicy)
{
    // Store the registry instance so the pointer outlives the caller's temporary.
    const SocialPolicyConfig_t& rCanonical = m_rRegistry.Get(rPolicy.id);
    m_pendingPolicies[rCanonical.category] = &rCanonical;
    m_revision.Bump();
}

void SocialEngineeringManager::SetPendingDraft(const SocialPolicyDraft_t& rDraft)
{
    for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
    {
        const SocialPolicyConfig_t& rCanonical = m_rRegistry.Get(rDraft.Get(category).id);
        m_pendingPolicies[category] = &rCanonical;
    }
    m_revision.Bump();
}

const SocialPolicyConfig_t& SocialEngineeringManager::GetActivePolicy(
    SocialCategory_t category) const
{
    const auto it = m_pendingPolicies.find(category);
    if (it == m_pendingPolicies.end() || !it->second)
    {
        throw std::runtime_error(
            "SocialEngineeringManager::GetActivePolicy: missing pending policy for category");
    }
    return *it->second;
}

const SocialPolicyConfig_t& SocialEngineeringManager::GetCommittedPolicy(
    SocialCategory_t category) const
{
    const auto it = m_committedPolicies.find(category);
    if (it == m_committedPolicies.end() || !it->second)
    {
        throw std::runtime_error(
            "SocialEngineeringManager::GetCommittedPolicy: missing committed policy for category");
    }
    return *it->second;
}

void SocialEngineeringManager::SetUpheavalPaid(int paid)
{
    if (paid < 0)
    {
        throw std::runtime_error("SocialEngineeringManager::SetUpheavalPaid: paid must be >= 0");
    }
    m_upheavalPaid = paid;
}

void SocialEngineeringManager::CommitPending()
{
    m_committedPolicies = m_pendingPolicies;
    m_upheavalPaid = 0;
}

std::vector<ActiveEffect_t> SocialEngineeringManager::CollectEffects() const
{
    // Policy effects pass through as they are; ratings accumulate per base later, since modifiers
    // come from any source.
    std::vector<ActiveEffect_t> result;
    for (const auto& [category, pPolicy] : m_pendingPolicies)
    {
        AppendActiveEffects(pPolicy->effects, nullptr, pPolicy->id, result);
    }
    return result;
}

const std::map<SocialRatingId_t, int>& SocialEngineeringManager::AccumulatedRatings_() const
{
    const uint64_t revision = m_revision.Get();
    if (m_cachedRatingsRevision != revision)
    {
        m_cachedRatings = AccumulateSocialRatings(CollectEffects(), std::nullopt);
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

} // namespace ac
