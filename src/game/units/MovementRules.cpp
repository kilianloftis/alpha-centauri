#include "game/units/MovementRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

namespace ac
{

bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject)
{
    if (rProjector.IsEmbarked())
    {
        return false;
    }
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
        // Air exerts on land and sea units, not on other air/orbital units.
        return rSubject.GetDomain() != UnitDomain_t::Air
            && rSubject.GetDomain() != UnitDomain_t::Orbital;
    case UnitDomain_t::Orbital:
        // Orbital projectors do not exert ZOC.
        return false;
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
    case UnitDomain_t::Orbital:
        return true;
    case UnitDomain_t::Sea:
        return rTile.IsWater();
    case UnitDomain_t::Land:
        return rTile.IsLand();
    }
    return false;
}

bool CanOccupyTileUnaided(const Unit& rMover, const Tile& rTile)
{
    if (CanEnterTileTerrain(rMover, rTile))
    {
        return true;
    }
    // A land unit garrisons a friendly sea base without needing a hull under it.
    return rMover.GetDomain() == UnitDomain_t::Land && rTile.IsWater()
        && HasFriendlyBase(rMover, rTile);
}

bool CanEnterTile(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap)
{
    if (CanOccupyTileUnaided(rMover, rTile))
    {
        return true;
    }
    if (rMover.GetDomain() != UnitDomain_t::Land || !rTile.IsWater())
    {
        return false;
    }
    if (FindBoardableTransport(rMover, rTile, rWorldMap))
    {
        return true;
    }
    EffectContext_t ctx;
    ctx.targetTile = &rTile;
    return HasPermission(rMover, PermissionId_t::Enter, ctx);
}

bool HasFriendlyOccupant(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap)
{
    const FactionId_t moverId = rMover.GetFaction().GetFactionId();
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit != &rMover && !pUnit->IsEmbarked()
            && pUnit->GetFaction().GetFactionId() == moverId)
        {
            return true;
        }
    }
    return false;
}

bool HasFriendlyBase(const Unit& rMover, const Tile& rTile)
{
    for (const BaseManager& rBase : rMover.GetFaction().Bases())
    {
        if (&rBase.GetTile() == &rTile)
        {
            return true;
        }
    }
    return false;
}

bool CanPlaceUnitOnTile(const Tile& rTile, const UnitPositionIndex& rPositions)
{
    return rPositions.CanPlaceUnit(rTile);
}

} // namespace ac
