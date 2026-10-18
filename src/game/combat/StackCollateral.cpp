#include "game/combat/StackCollateral.h"

#include "game/combat/CombatRules.h"
#include "game/combat/PlanetPearls.h"
#include "game/faction/Faction.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"

#include <vector>

namespace ac
{

void ApplyStackCollateral(WorldMap& rWorldMap, const MoraleCalculator& rMorale,
                          Unit& rAttacker, Unit& rDefender)
{
    const std::vector<CollateralHit_t> plan = PlanStackCollateral(rAttacker, rDefender, rWorldMap);
    for (const CollateralHit_t& rHit : plan)
    {
        Unit& rOccupant = *rHit.pOccupant;
        if (rHit.bDestroyed)
        {
            GrantPlanetPearls(rMorale, rAttacker.GetFaction(), rOccupant);
            rOccupant.GetFaction().GetUnitManager().DestroyUnit(rOccupant);
            continue;
        }
        if (rHit.damage == 0)
        {
            continue;
        }
        rOccupant.SetCurrentHp(rOccupant.GetCurrentHp() - rHit.damage);
        if (rOccupant.GetCurrentHp() <= 0)
        {
            rOccupant.GetFaction().GetUnitManager().DestroyUnit(rOccupant);
        }
    }

    const std::vector<Unit*> unprotected =
        NonCombatantsLeftUnprotected(rAttacker, rDefender, rWorldMap, plan);
    for (Unit* pNonCombatant : unprotected)
    {
        pNonCombatant->GetFaction().GetUnitManager().DestroyUnit(*pNonCombatant);
    }
}

} // namespace ac
