#include "game/movement/FuelRules.h"

#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementConstants.h"
#include "game/movement/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitOrder.h"

namespace ac
{

bool IsRefuelSite(const Unit& rUnit, const WorldMap& rWorldMap)
{
    // Pad improvements (Base, Airbase): territory must harbor the unit's domain.
    if (TileHarbors(rUnit.GetTile(), rUnit.GetDomain(), rUnit.GetFaction().GetFactionId(),
                    rWorldMap.GetTerritory()))
    {
        return true;
    }

    // Carriers: only landed cargo of a domain the carrier carries.
    if (!rUnit.IsEmbarked())
    {
        return false;
    }
    const Unit* pCarrier = rUnit.GetCarrier();
    return pCarrier
        && UnitCarries(*pCarrier, rUnit.GetDomain(), rUnit.GetFaction().GetFactionId());
}

int OutOfFuelDamage(const Unit& rUnit)
{
    const int maxHp = ResolveStat(rUnit, StatId_t::HitPoints);
    const int damagePercent = ResolveStat(rUnit, StatId_t::DamageFromOutOfFuel);
    return FinalizeResolvedStat(maxHp * (damagePercent / 100.0));
}

namespace
{

// Mirrors ProcessFuelAtTurnEnd's away-from-pad outcome without mutating: after optional
// carrier land, would remaining-move fuel burn hit 0 and out-of-fuel damage kill the unit?
bool WouldBeDestroyedWithoutRefuelThisTurn_(const Unit& rUnit, const WorldMap& rWorldMap)
{
    if (!rUnit.GetDesign().UsesFuel())
    {
        return false;
    }

    if (!rUnit.IsEmbarked() && rUnit.GetDomain() == UnitDomain_t::Air
        && FindBoardableTransport(rUnit, rUnit.GetTile(), rWorldMap))
    {
        return false;
    }

    if (IsRefuelSite(rUnit, rWorldMap))
    {
        return false;
    }

    const int movesRemaining =
        rUnit.GetMoveFragmentsRemaining() / MovementConstants_t::k_moveFragmentsPerPoint;
    if (rUnit.GetCurrentFuel() - movesRemaining > 0)
    {
        return false;
    }

    return rUnit.GetCurrentHp() - OutOfFuelDamage(rUnit) <= 0;
}

} // namespace

bool NeedsAutoReturnToFuel(const Unit& rUnit, const WorldMap& rWorldMap)
{
    if (rUnit.GetOrder().has_value() || rUnit.GetMoveFragmentsRemaining() <= 0)
    {
        return false;
    }

    return WouldBeDestroyedWithoutRefuelThisTurn_(rUnit, rWorldMap);
}

} // namespace ac
