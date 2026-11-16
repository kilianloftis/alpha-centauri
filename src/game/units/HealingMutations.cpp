#include "game/units/HealingMutations.h"

#include "game/units/HealingRules.h"
#include "game/units/Unit.h"

namespace ac
{

void ApplyIdleHeal(Unit& rUnit)
{
    const int newHp = IdleHealNewHp(rUnit);
    if (newHp > rUnit.GetCurrentHp())
    {
        rUnit.SetCurrentHp(newHp);
    }
}

} // namespace ac
