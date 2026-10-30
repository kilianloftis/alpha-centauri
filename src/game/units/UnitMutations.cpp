#include "game/units/UnitMutations.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"

namespace ac
{

bool ExpendIfSingleUse(Unit& rUnit)
{
    if (!ResolveFlag(rUnit, RuleFlagId_t::SingleUse))
    {
        return false;
    }
    rUnit.GetFaction().GetUnitManager().DestroyUnit(rUnit);
    return true;
}

} // namespace ac
