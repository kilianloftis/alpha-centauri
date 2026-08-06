#include "game/council/CouncilEffects.h"

#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"

#include <span>
#include <variant>

namespace ac
{

namespace
{

// Wrappers borrow the registry/rules config, so append one entry at a time from the
// source's own storage rather than copying it into a local vector first.
void AppendBorrowed_(const EffectConfig_t& rEffect, const std::string& rSourceId,
                     std::vector<ActiveEffect_t>& rOut)
{
    AppendActiveEffects(std::span<const EffectConfig_t>(&rEffect, 1), nullptr, rSourceId, rOut);
}

} // namespace

bool IsContinuousWorldEffect(const EffectConfig_t& rEffect)
{
    return rEffect.persistence == EffectPersistence_t::Continuous
           && rEffect.scope == EffectScope_t::WorldGlobal;
}

void CouncilEffects::RebuildWorld(const std::vector<std::string>& rActiveProposalIds,
                                  const CouncilProposalRegistry& rRegistry)
{
    m_worldEffects.clear();
    for (const std::string& rId : rActiveProposalIds)
    {
        const CouncilProposalConfig_t* pConfig = rRegistry.Find(rId);
        if (!pConfig)
        {
            continue;
        }
        for (const EffectConfig_t& rEffect : pConfig->effects)
        {
            if (IsContinuousWorldEffect(rEffect))
            {
                AppendBorrowed_(rEffect, "council", m_worldEffects);
            }
        }
    }
}

void CouncilEffects::SetGovernorEffects(const std::vector<EffectConfig_t>& rGovernorEffects)
{
    m_governorEffects.clear();
    for (const EffectConfig_t& rEffect : rGovernorEffects)
    {
        if (rEffect.persistence == EffectPersistence_t::Continuous
            && rEffect.scope == EffectScope_t::FactionGlobal)
        {
            AppendBorrowed_(rEffect, "council_governor", m_governorEffects);
        }
    }
}

bool CouncilEffects::HasActiveRuleFlag(RuleFlagId_t flag) const
{
    for (const ActiveEffect_t& rEffect : m_worldEffects)
    {
        if (const auto* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect))
        {
            if (pFlag->flag == flag)
            {
                return true;
            }
        }
    }
    return false;
}

} // namespace ac
