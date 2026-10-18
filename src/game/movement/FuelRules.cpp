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

// IsRefuelSite as it will read once rUnit is aboard rCarrier.
bool RefuelsAboard_(const Unit& rUnit, const Unit& rCarrier, const WorldMap& rWorldMap)
{
    const FactionId_t factionId = rUnit.GetFaction().GetFactionId();
    return TileHarbors(rUnit.GetTile(), rUnit.GetDomain(), factionId, rWorldMap.GetTerritory())
           || UnitCarries(rCarrier, rUnit.GetDomain(), factionId);
}

} // namespace

EndOfTurnFuel_t ResolveEndOfTurnFuel(const Unit& rUnit, const WorldMap& rWorldMap)
{
    EndOfTurnFuel_t fuel;
    if (!rUnit.GetDesign().UsesFuel())
    {
        return fuel;
    }

    const Unit* pCarrier = nullptr;
    if (!rUnit.IsEmbarked() && rUnit.GetDomain() == UnitDomain_t::Air)
    {
        pCarrier = FindBoardableTransport(rUnit, rUnit.GetTile(), rWorldMap);
    }
    fuel.bBoardsCarrier = pCarrier != nullptr;
    fuel.bRefuels = pCarrier != nullptr ? RefuelsAboard_(rUnit, *pCarrier, rWorldMap)
                                        : IsRefuelSite(rUnit, rWorldMap);
    if (fuel.bRefuels)
    {
        return fuel;
    }

    fuel.bBurnsFuel = true;
    const int pointsLeft =
        rUnit.GetMoveFragmentsRemaining() / MovementConstants_t::k_moveFragmentsPerPoint;
    if (rUnit.GetCurrentFuel() - pointsLeft > 0)
    {
        return fuel;
    }
    fuel.bOutOfFuel = true;
    fuel.outOfFuelDamage = OutOfFuelDamage(rUnit);
    fuel.bDestroyed = rUnit.GetCurrentHp() - fuel.outOfFuelDamage <= 0;
    return fuel;
}

bool NeedsAutoReturnToFuel(const Unit& rUnit, const WorldMap& rWorldMap)
{
    if (rUnit.GetOrder().has_value() || rUnit.GetMoveFragmentsRemaining() <= 0)
    {
        return false;
    }
    return ResolveEndOfTurnFuel(rUnit, rWorldMap).bDestroyed;
}

} // namespace ac
