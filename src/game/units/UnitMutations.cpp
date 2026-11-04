#include "game/units/UnitMutations.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"

namespace ac
{

bool SpendAttack(Unit& rAttacker, AttackKind_t kind)
{
    rAttacker.Latches().MarkAttacked();
    rAttacker.SpendMoveFragments(AttackMoveFragments(rAttacker, kind));
    rAttacker.ClearOrder();
    // Destroy Enhancement never expends SingleUse (terranx-map.md Destroy Enhancement).
    if (kind == AttackKind_t::DestroyEnhancement)
    {
        return false;
    }
    return ExpendIfSingleUse(rAttacker);
}

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
