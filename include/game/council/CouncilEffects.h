#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/effects/BonusEffect.h"
#include "game/effects/EffectEnums.h"

#include <memory>
#include <string>
#include <vector>

namespace ac
{

class CouncilProposalRegistry;

// True when an effect is a continuous, world-global council effect — the effects a passed
// proposal projects onto the whole map (Trade Pact, U.N. Charter, etc.). Shared by the
// effect store (which gathers them) and proposal eligibility (which tests for their
// presence).
bool IsContinuousWorldEffect(const EffectConfig_t& rEffect);

// Owns the continuous ActiveEffects the Planetary Council projects: world-global effects
// from the proposals currently in force, and faction-global effects granted to the
// Planetary Governor. Backing EffectConfig_t nodes are heap-allocated; on rebuild, prior
// nodes are retired (not destroyed) so ActiveEffect_t::config addresses remain valid for
// the lifetime of this CouncilEffects instance, including across RebuildWorld /
// SetGovernorEffects.
class CouncilEffects
{
public:
    // Rebuild the world lane from the continuous WorldGlobal effects of the given active
    // proposals (ids resolved through the registry; unknown ids are skipped).
    void RebuildWorld(const std::vector<std::string>& rActiveProposalIds,
                      const CouncilProposalRegistry& rRegistry);

    // Set the governor lane from the continuous FactionGlobal effects in the council rules.
    void SetGovernorEffects(const std::vector<EffectConfig_t>& rGovernorEffects);

    const std::vector<ActiveEffect_t>& WorldEffects() const { return m_worldEffects; }
    const std::vector<ActiveEffect_t>& GovernorEffects() const { return m_governorEffects; }

    // True when any active world effect carries the given RuleFlag. Throws if a wrapper
    // has a null config (invariant violation — configs are always set at rebuild).
    bool HasActiveRuleFlag(RuleFlagId_t flag) const;

private:
    void RebuildWrappers_();
    void RetireConfigs_(std::vector<std::unique_ptr<EffectConfig_t>>& rLive,
                        std::vector<std::unique_ptr<EffectConfig_t>>& rRetired);

    std::vector<std::unique_ptr<EffectConfig_t>> m_worldConfigs;
    std::vector<std::unique_ptr<EffectConfig_t>> m_retiredWorldConfigs;
    std::vector<ActiveEffect_t> m_worldEffects;
    std::vector<std::unique_ptr<EffectConfig_t>> m_governorConfigs;
    std::vector<std::unique_ptr<EffectConfig_t>> m_retiredGovernorConfigs;
    std::vector<ActiveEffect_t> m_governorEffects;
};

} // namespace ac
