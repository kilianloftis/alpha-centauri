#include "game/units/MovementRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/InteractionResolve.h"
#include "game/faction/DiplomaticPermissionRules.h"
#include "game/map/ImprovementIds.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

namespace ac
{
namespace
{

InteractionSurface_t SurfaceOf_(const Tile& rTile)
{
    return rTile.IsWater() ? InteractionSurface_t::Water : InteractionSurface_t::Land;
}

InteractionQuery_t EnterQuery_(const Unit& rMover, const Tile& rTile)
{
    InteractionQuery_t q;
    q.grid = InteractionGridId_t::Enter;
    q.actorDomain = rMover.GetDomain();
    q.surface = SurfaceOf_(rTile);
    return q;
}

} // namespace

bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject,
                     const InteractionGridsConfig_t& rGrids)
{
    if (rProjector.IsEmbarked())
    {
        return false;
    }
    if (rProjector.GetFaction().GetFactionId() == rSubject.GetFaction().GetFactionId())
    {
        return false;
    }
    // The subject is the actor: it is the unit whose move is being gated, and overrides
    // resolve from the acting unit. That is what lets a unit declare "nothing holds me"
    // (Cloaking Device, Probe Team) as a zoc deny on itself — non-default where stock allows.
    InteractionQuery_t q;
    q.grid = InteractionGridId_t::Zoc;
    q.actorDomain = rSubject.GetDomain();
    q.targetDomain = rProjector.GetDomain();
    EffectContext_t ctx;
    return ResolveInteractionCell(rGrids, q, &rSubject, ctx) == InteractionCell_t::Allow;
}

bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile,
                         const InteractionGridsConfig_t& rGrids)
{
    EffectContext_t ctx;
    ctx.targetTile = &rTile;
    return ResolveInteractionCell(rGrids, EnterQuery_(rMover, rTile), &rMover, ctx)
        == InteractionCell_t::Allow;
}

bool CanHoldTileWithoutCarrier(const Unit& rMover, const Tile& rTile,
                               const WorldMap& rWorldMap,
                               const InteractionGridsConfig_t& rGrids)
{
    if (CanEnterTileTerrain(rMover, rTile, rGrids))
    {
        return true;
    }
    // Any unit may *hold* a tile that harbors its domain. This grants no entry: a land unit
    // still reaches its own sea base only by transport or pods.
    return TileHarbors(rTile, rMover.GetDomain(), rMover.GetFaction().GetFactionId(),
                       rWorldMap.GetTerritory());
}

bool CanPhysicallyEnterTile(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                            const InteractionGridsConfig_t& rGrids)
{
    EffectContext_t ctx;
    ctx.targetTile = &rTile;
    if (ResolveInteractionCell(rGrids, EnterQuery_(rMover, rTile), &rMover, ctx)
        == InteractionCell_t::Allow)
    {
        return true;
    }
    // Port rule: a ship may berth where the tile harbors sea even though the enter grid
    // refuses its land tile. "Coastal" needs no test of its own — movement is step by step
    // between adjacent tiles, so a ship can only arrive from water it already occupies.
    if (rMover.GetDomain() == UnitDomain_t::Sea && !rTile.IsWater()
        && TileHarbors(rTile, UnitDomain_t::Sea, rMover.GetFaction().GetFactionId(),
                       rWorldMap.GetTerritory()))
    {
        return true;
    }
    // Water is reached only by boarding. A land unit gets to its own sea base by transport
    // too, so TileHarbors is deliberately not consulted here — it governs only whether a
    // unit already there may stay (CanHoldTileWithoutCarrier / SurvivesCarrierLoss).
    return rMover.GetDomain() == UnitDomain_t::Land && rTile.IsWater()
        && FindBoardableTransport(rMover, rTile, rWorldMap) != nullptr;
}

bool CanEnterTile(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                  const InteractionGridsConfig_t& rGrids)
{
    return CanPhysicallyEnterTile(rMover, rTile, rWorldMap, rGrids)
        && MayEnterTerritoryOf(rMover, rWorldMap.GetTerritory().GetOwner(rTile));
}

bool HasFriendlyOccupant(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap)
{
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit != &rMover
            && MayShareTiles(rMover.GetFaction(), pUnit->GetFaction().GetFactionId()))
        {
            return true;
        }
    }
    return false;
}

bool HasFriendlyBase(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap)
{
    if (!rTile.HasImprovement(ImprovementIds::k_Base))
    {
        return false;
    }
    const FactionId_t owner = rWorldMap.GetTerritory().GetOwner(rTile);
    return owner != k_NoFactionOwner && MayShareTiles(rMover.GetFaction(), owner);
}

bool CanPlaceUnitOnTile(const Tile& rTile, const UnitPositionIndex& rPositions)
{
    return rPositions.CanPlaceUnit(rTile);
}

} // namespace ac
