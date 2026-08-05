#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/effects/BonusEffect.h"
#include "game/effects/EffectEnums.h"

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
// Planetary Governor. Keeps each ActiveEffect_t wrapper together with its backing
// EffectConfig_t, so the wrappers this class hands out are internally consistent.
//
// That consistency does NOT survive a rebuild: RebuildWorld / SetGovernorEffects clear and
// refill the config vectors, reallocating them, so any ActiveEffect_t *copied out* before
// the rebuild dangles. Callers that retain copies (Faction's composed pool, BaseManager's
// memo) rely on PlanetaryCouncil bumping its revision on every rebuild — see
// PlanetaryCouncil::GetRevision. Package 3 replaces this with stable storage.
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

    // True when any active world effect carries the given RuleFlag.
    bool HasActiveRuleFlag(RuleFlagId_t flag) const;

private:
    void RebuildWrappers_();

    std::vector<EffectConfig_t> m_worldConfigs;
    std::vector<ActiveEffect_t> m_worldEffects;
    std::vector<EffectConfig_t> m_governorConfigs;
    std::vector<ActiveEffect_t> m_governorEffects;
};

} // namespace ac
