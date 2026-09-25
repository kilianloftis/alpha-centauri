#include "game/effects/CoexistenceResolve.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/units/Unit.h"

namespace ac
{

std::vector<CoexistenceOverrideEffect_t> ActiveCoexistenceOverrides(const Unit& rUnit)
{
    std::vector<CoexistenceOverrideEffect_t> overrides;
    const EffectContext_t context;
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rUnit).effects)
    {
        const CoexistenceOverrideEffect_t* pOverride =
            std::get_if<CoexistenceOverrideEffect_t>(&rEffect.config->effect);
        if (!pOverride)
        {
            continue;
        }
        if (rEffect.config->scope != EffectScope_t::ThisUnit
            && rEffect.config->scope != EffectScope_t::FactionUnits)
        {
            continue;
        }
        if (!ConditionSatisfied(*rEffect.config, context, rEffect.originBase))
        {
            continue;
        }
        overrides.push_back(*pOverride);
    }
    return overrides;
}

} // namespace ac
