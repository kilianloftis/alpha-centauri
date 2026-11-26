#include "game/effects/CoexistenceResolve.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/units/Unit.h"

namespace ac
{

namespace
{

// Both override kinds select the same way: unit-scoped, condition satisfied. Collecting one
// payload type is the only thing that differs.
template <typename Payload_t>
std::vector<Payload_t> ActiveUnitOverrides_(const Unit& rUnit)
{
    std::vector<Payload_t> overrides;
    const EffectContext_t context;
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rUnit).effects)
    {
        const Payload_t* pOverride = std::get_if<Payload_t>(&rEffect.config->effect);
        if (!pOverride)
        {
            continue;
        }
        if (rEffect.config->scope != EffectScope_t::ThisUnit
            && rEffect.config->scope != EffectScope_t::FactionUnits)
        {
            continue;
        }
        if (!ConditionSatisfied(*rEffect.config, ActiveEffectContext(rEffect, context),
                                rEffect.originBase))
        {
            continue;
        }
        overrides.push_back(*pOverride);
    }
    return overrides;
}

} // namespace

std::vector<CoexistenceOverrideEffect_t> ActiveCoexistenceOverrides(const Unit& rUnit)
{
    return ActiveUnitOverrides_<CoexistenceOverrideEffect_t>(rUnit);
}

std::vector<TerraformBlockOverrideEffect_t> ActiveTerraformBlockOverrides(const Unit& rUnit)
{
    return ActiveUnitOverrides_<TerraformBlockOverrideEffect_t>(rUnit);
}

} // namespace ac
