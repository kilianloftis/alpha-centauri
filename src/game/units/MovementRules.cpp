#include "game/units/MovementRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"

namespace ac
{

namespace
{

bool s_bSingleUnitPerTile = false;

} // namespace

bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject)
{
    if (rProjector.GetFaction().GetFactionId() == rSubject.GetFaction().GetFactionId())
    {
        return false;
    }
    // Air subjects are already excluded by the domain match below; this flag is for
    // land/sea units (e.g. probes) that ignore ZOC without changing domain.
    if (ResolveFlag(rSubject, RuleFlagId_t::IgnoreZoneOfControl))
    {
        return false;
    }

    switch (rProjector.GetDomain())
    {
    case UnitDomain_t::Air:
        // Air exerts on land and sea units, not on other air units.
        return rSubject.GetDomain() != UnitDomain_t::Air;
    case UnitDomain_t::Sea:
        return rSubject.GetDomain() == UnitDomain_t::Sea;
    case UnitDomain_t::Land:
        return rSubject.GetDomain() == UnitDomain_t::Land;
    }
    return false;
}

bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile)
{
    switch (rMover.GetDomain())
    {
    case UnitDomain_t::Air:
        return true;
    case UnitDomain_t::Sea:
        return rTile.IsWater();
    case UnitDomain_t::Land:
        return rTile.IsLand();
    }
    return false;
}

bool UsesFungusEntryRules(const Unit& rMover, const Tile& rTile)
{
    return rTile.GetHasFungus() && !ResolveFlag(rMover, RuleFlagId_t::TreatFungusAsRoad);
}

bool HasFriendlyOccupant(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap)
{
    const FactionId_t moverId = rMover.GetFaction().GetFactionId();
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit != &rMover && pUnit->GetFaction().GetFactionId() == moverId)
        {
            return true;
        }
    }
    return false;
}

void SetSingleUnitPerTile(bool bSingleUnitPerTile)
{
    s_bSingleUnitPerTile = bSingleUnitPerTile;
}

bool IsSingleUnitPerTile()
{
    return s_bSingleUnitPerTile;
}

bool CanPlaceUnitOnTile(const Tile& rTile, const UnitPositionIndex& rPositions)
{
    return !IsSingleUnitPerTile() || rPositions.GetUnitsOnTile(rTile).empty();
}

} // namespace ac
