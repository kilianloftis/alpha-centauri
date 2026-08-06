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

// The continuous ActiveEffects the Planetary Council projects: world-global effects from
// the proposals currently in force, and faction-global effects granted to the Planetary
// Governor. Holds no config storage of its own — wrappers point straight at the proposal
// registry / council rules that own the EffectConfig_t entries, exactly as building and
// pop-type effects point at their registries. Those sources are loaded once and outlive the
// council, so ActiveEffect_t::config addresses are stable for the whole session: a retained
// wrapper stays readable across any number of rebuilds, and an unchanged proposal keeps the
// same config address. The effect *vectors* are rebuilt in place, so WorldEffects() /
// GovernorEffects() references (and iterators) are invalidated by the matching setter —
// copy out, or re-read after a rebuild.
class CouncilEffects
{
public:
    // Rebuild the world lane from the continuous WorldGlobal effects of the given active
    // proposals (ids resolved through the registry; unknown ids are skipped).
    // rRegistry must outlive this CouncilEffects — the wrappers borrow its configs.
    void RebuildWorld(const std::vector<std::string>& rActiveProposalIds,
                      const CouncilProposalRegistry& rRegistry);

    // Set the governor lane from the continuous FactionGlobal effects in the council rules.
    // rGovernorEffects must outlive this CouncilEffects (it is the rules config's own
    // vector) — the wrappers borrow its entries, so temporaries are rejected.
    void SetGovernorEffects(const std::vector<EffectConfig_t>& rGovernorEffects);
    void SetGovernorEffects(std::vector<EffectConfig_t>&& rGovernorEffects) = delete;

    const std::vector<ActiveEffect_t>& WorldEffects() const { return m_worldEffects; }
    const std::vector<ActiveEffect_t>& GovernorEffects() const { return m_governorEffects; }

    // True when any active world effect carries the given RuleFlag.
    bool HasActiveRuleFlag(RuleFlagId_t flag) const;

private:
    std::vector<ActiveEffect_t> m_worldEffects;
    std::vector<ActiveEffect_t> m_governorEffects;
};

} // namespace ac
