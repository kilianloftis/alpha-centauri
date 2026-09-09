#include "game/units/MovementRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/InteractionResolve.h"
#include "game/faction/base/BaseManager.h"
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
    InteractionQuery_t q;
    q.grid = InteractionGridId_t::Zoc;
    q.actorDomain = rProjector.GetDomain();
    q.targetDomain = rSubject.GetDomain();
    EffectContext_t ctx;
    if (ResolveInteractionCell(rGrids, q, &rProjector, ctx, nullptr, nullptr,
                               rProjector.GetFaction().GetFactionId())
        != InteractionCell_t::Allow)
    {
        return false;
    }

    // Air subjects are already excluded by the stock zoc grid; this flag is for land/sea
    // units (e.g. probes) that ignore ZOC without changing domain. Resolved last because it
    // collects the subject's effects — only worth paying once the grid says ZOC applies.
    return !ResolveFlag(rSubject, RuleFlagId_t::IgnoreZoneOfControl);
}

bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile,
                         const InteractionGridsConfig_t& rGrids)
{
    EffectContext_t ctx;
    ctx.targetTile = &rTile;
    ctx.pUnit = &rMover;
    return ResolveInteractionCell(rGrids, EnterQuery_(rMover, rTile), &rMover, ctx, &rTile,
                                  nullptr, rMover.GetFaction().GetFactionId())
        == InteractionCell_t::Allow;
}

bool CanOccupyTileUnaided(const Unit& rMover, const Tile& rTile,
                          const InteractionGridsConfig_t& rGrids)
{
    if (CanEnterTileTerrain(rMover, rTile, rGrids))
    {
        return true;
    }
    // A land unit garrisons a friendly sea base without needing a hull under it.
    return rMover.GetDomain() == UnitDomain_t::Land && rTile.IsWater()
        && HasFriendlyBase(rMover, rTile);
}

bool CanEnterTile(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                  const InteractionGridsConfig_t& rGrids)
{
    EffectContext_t ctx;
    ctx.targetTile = &rTile;
    ctx.pUnit = &rMover;
    // Widest source set: the mover's own overrides, the tile's improvements and features,
    // and ThisTile overrides projected by units already standing here.
    if (ResolveInteractionCell(rGrids, EnterQuery_(rMover, rTile), &rMover, ctx, &rTile,
                               &rWorldMap, rMover.GetFaction().GetFactionId())
        == InteractionCell_t::Allow)
    {
        return true;
    }
    // That resolve ran the same query CanOccupyTileUnaided would, over a superset of the
    // sources, so re-running it here would only repeat the work. Boarding is the one
    // remaining way onto water: a land unit reaches even its own sea base by transport, so
    // HasFriendlyBase is deliberately *not* consulted here. It still governs whether a unit
    // already there may stay (CanOccupyTileUnaided / SurvivesCarrierLoss).
    if (rMover.GetDomain() != UnitDomain_t::Land || !rTile.IsWater())
    {
        return false;
    }
    return FindBoardableTransport(rMover, rTile, rWorldMap) != nullptr;
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
