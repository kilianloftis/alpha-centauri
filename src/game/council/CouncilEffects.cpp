#include "game/council/CouncilEffects.h"

#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"

#include <span>
#include <stdexcept>
#include <utility>
#include <variant>

namespace ac
{

bool IsContinuousWorldEffect(const EffectConfig_t& rEffect)
{
    return rEffect.persistence == EffectPersistence_t::Continuous
           && rEffect.scope == EffectScope_t::WorldGlobal;
}

void CouncilEffects::RetireConfigs_(std::vector<std::unique_ptr<EffectConfig_t>>& rLive,
                                    std::vector<std::unique_ptr<EffectConfig_t>>& rRetired)
{
    // Move nodes aside without destroying them so previously handed-out config* stay valid.
    for (std::unique_ptr<EffectConfig_t>& pConfig : rLive)
    {
        rRetired.push_back(std::move(pConfig));
    }
    rLive.clear();
}

void CouncilEffects::RebuildWorld(const std::vector<std::string>& rActiveProposalIds,
                                  const CouncilProposalRegistry& rRegistry)
{
    RetireConfigs_(m_worldConfigs, m_retiredWorldConfigs);
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
                m_worldConfigs.push_back(std::make_unique<EffectConfig_t>(rEffect));
            }
        }
    }
    RebuildWrappers_();
}

void CouncilEffects::SetGovernorEffects(const std::vector<EffectConfig_t>& rGovernorEffects)
{
    RetireConfigs_(m_governorConfigs, m_retiredGovernorConfigs);
    for (const EffectConfig_t& rEffect : rGovernorEffects)
    {
        if (rEffect.persistence == EffectPersistence_t::Continuous
            && rEffect.scope == EffectScope_t::FactionGlobal)
        {
            m_governorConfigs.push_back(std::make_unique<EffectConfig_t>(rEffect));
        }
    }
    RebuildWrappers_();
}

void CouncilEffects::RebuildWrappers_()
{
    m_worldEffects.clear();
    for (const std::unique_ptr<EffectConfig_t>& pEffect : m_worldConfigs)
    {
        AppendActiveEffects(std::span<const EffectConfig_t>(pEffect.get(), 1),
                            nullptr, "council", m_worldEffects);
    }

    m_governorEffects.clear();
    for (const std::unique_ptr<EffectConfig_t>& pEffect : m_governorConfigs)
    {
        AppendActiveEffects(std::span<const EffectConfig_t>(pEffect.get(), 1),
                            nullptr, "council_governor", m_governorEffects);
    }
}

bool CouncilEffects::HasActiveRuleFlag(RuleFlagId_t flag) const
{
    for (const ActiveEffect_t& rEffect : m_worldEffects)
    {
        if (!rEffect.config)
        {
            throw std::runtime_error("CouncilEffects::HasActiveRuleFlag: null config");
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
