#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"

#include <string>
#include <vector>

namespace ac
{

class CouncilProposalRegistry;

// A continuous world-global effect, as a passed proposal projects over the map.
bool IsContinuousWorldEffect(const EffectConfig_t& rEffect);

// The council's continuous effects: laws in force and the Governor's grants. Points at the
// registry's configs, which are stable; the vectors rebuild in place, so re-read after one.
class CouncilEffects
{
public:
    // Rebuilds the world lane from active proposals; unknown ids are skipped. rRegistry must
    // outlive this.
    void RebuildWorld(const std::vector<std::string>& rActiveProposalIds,
                      const CouncilProposalRegistry& rRegistry);

    // The Governor lane from the rules' FactionGlobal effects, borrowed, so not a temporary.
    void SetGovernorEffects(const std::vector<EffectConfig_t>& rGovernorEffects);
    void SetGovernorEffects(std::vector<EffectConfig_t>&& rGovernorEffects) = delete;

    // When the office falls vacant (a governor expelled for a major atrocity).
    void ClearGovernorEffects();

    const std::vector<ActiveEffect_t>& WorldEffects() const { return m_worldEffects; }
    const std::vector<ActiveEffect_t>& GovernorEffects() const { return m_governorEffects; }

    // True when any active world effect carries the given RuleFlag.
    bool HasActiveRuleFlag(RuleFlagId_t flag) const;

private:
    std::vector<ActiveEffect_t> m_worldEffects;
    std::vector<ActiveEffect_t> m_governorEffects;
};

} // namespace ac
