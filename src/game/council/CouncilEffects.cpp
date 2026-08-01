#include "game/council/CouncilEffects.h"

#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"

#include <variant>

namespace ac
{

bool IsContinuousWorldEffect(const EffectConfig_t& rEffect)
{
    return rEffect.persistence == EffectPersistence_t::Continuous
           && rEffect.scope == EffectScope_t::WorldGlobal;
}

void CouncilEffects::RebuildWorld(const std::vector<std::string>& rActiveProposalIds,
                                  const CouncilProposalRegistry& rRegistry)
{
    m_worldConfigs.clear();
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
                m_worldConfigs.push_back(rEffect);
            }
        }
    }
    RebuildWrappers_();
}

void CouncilEffects::SetGovernorEffects(const std::vector<EffectConfig_t>& rGovernorEffects)
{
    m_governorConfigs.clear();
    for (const EffectConfig_t& rEffect : rGovernorEffects)
    {
        if (rEffect.persistence == EffectPersistence_t::Continuous
            && rEffect.scope == EffectScope_t::FactionGlobal)
        {
            m_governorConfigs.push_back(rEffect);
        }
    }
    RebuildWrappers_();
}

void CouncilEffects::RebuildWrappers_()
{
    // Backing configs live in the member vectors, so ActiveEffect_t::config pointers into
    // them stay valid until the next rebuild.
    m_worldEffects.clear();
    AppendActiveEffects(m_worldConfigs, nullptr, "council", m_worldEffects);

    m_governorEffects.clear();
    AppendActiveEffects(m_governorConfigs, nullptr, "council_governor", m_governorEffects);
}

bool CouncilEffects::HasActiveRuleFlag(RuleFlagId_t flag) const
{
    for (const ActiveEffect_t& rEffect : m_worldEffects)
    {
        if (!rEffect.config)
        {
            continue;
        }
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
