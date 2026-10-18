#include "game/combat/ExplosionRules.h"

#include "game/effects/TriggeredEffect.h"
#include "game/units/Unit.h"

namespace ac
{

std::optional<int> ExplosionRadius(const ExplosionEffect_t& rConfig, const Unit* pDetonator)
{
    if (!rConfig.radiusStat)
    {
        return rConfig.radius;
    }
    if (pDetonator == nullptr)
    {
        return std::nullopt;
    }
    return pDetonator->GetStat(*rConfig.radiusStat);
}

} // namespace ac
