#include "game/units/AttackRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/InteractionResolve.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementRules.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

namespace ac
{

namespace
{

// Harbor pad, or embarked on a same-faction carrier that carries it. Co-located but
// unembarked air over a deck is not resting (same rule as IsRefuelSite).
bool DefenderIsResting_(const Unit& rDefender, const Tile& rTile, const WorldMap& rWorldMap)
{
    const FactionId_t factionId = rDefender.GetFaction().GetFactionId();
    const UnitDomain_t domain = rDefender.GetDomain();
    if (TileHarbors(rTile, domain, factionId, rWorldMap.GetTerritory()))
    {
        return true;
    }
    if (!rDefender.IsEmbarked())
    {
        return false;
    }
    const Unit* pCarrier = rDefender.GetCarrier();
    return pCarrier && UnitCarries(*pCarrier, domain, factionId);
}

} // namespace

bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile, const WorldMap& rWorldMap,
                   const InteractionGridsConfig_t& rGrids)
{
    // No attacking a tile the unit could not enter...
    if (!CanEnterTile(rAttacker, rTargetTile, rWorldMap, rGrids))
    {
        return false;
    }

    // ...but entry is not sufficient: whether the attacker may strike at all from the ground
    // it is standing on is its own question, so an amphibious override opens the assault
    // without also opening ocean movement.
    InteractionQuery_t q;
    q.grid = InteractionGridId_t::AttackTile;
    q.actorDomain = rAttacker.GetDomain();
    q.footing = FootingFor(rAttacker);
    EffectContext_t ctx;
    ctx.pAttacker = &rAttacker;
    ctx.targetTile = &rTargetTile;
    return ResolveInteractionCell(rGrids, q, &rAttacker, ctx) == InteractionCell_t::Allow;
}

Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile,
                               const WorldMap& rWorldMap,
                               const TileEffectsContext& rTileEffects)
{
    const Faction& rObserverFaction = rObserver.GetFaction();
    const FactionId_t observerId = rObserverFaction.GetFactionId();
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != observerId
            && IsUnitVisibleTo(rObserverFaction, *pUnit, rTileEffects))
        {
            return pUnit;
        }
    }
    // Embarked cargo defends only in a base; prefer a non-embarked hostile above.
    if (!rTile.HasImprovement(ImprovementIds::k_Base))
    {
        return nullptr;
    }
    for (Unit* pUnit : rWorldMap.GetCargoOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != observerId
            && IsUnitVisibleTo(rObserverFaction, *pUnit, rTileEffects))
        {
            return pUnit;
        }
    }
    return nullptr;
}

Unit* FindAttackableHostileOnTile(const Unit& rAttacker, const Tile& rTargetTile,
                                  const WorldMap& rWorldMap,
                                  const TileEffectsContext& rTileEffects)
{
    const InteractionGridsConfig_t& rGrids = rTileEffects.GetInteractionGrids();
    if (rAttacker.GetMoveFragmentsRemaining() <= 0)
    {
        return nullptr;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile, rWorldMap.GetWidth()))
    {
        return nullptr;
    }
    if (!CanAttackTile(rAttacker, rTargetTile, rWorldMap, rGrids))
    {
        return nullptr;
    }
    Unit* pDefender =
        FindVisibleHostileOnTile(rAttacker, rTargetTile, rWorldMap, rTileEffects);
    if (!pDefender)
    {
        return nullptr;
    }

    InteractionQuery_t q;
    q.grid = InteractionGridId_t::AttackUnit;
    q.actorDomain = rAttacker.GetDomain();
    q.targetDomain = pDefender->GetDomain();
    EffectContext_t ctx;
    ctx.pAttacker = &rAttacker;
    ctx.targetTile = &rTargetTile;
    if (ResolveInteractionCell(rGrids, q, &rAttacker, ctx) == InteractionCell_t::Deny
        && !DefenderIsResting_(*pDefender, rTargetTile, rWorldMap))
    {
        return nullptr;
    }
    return pDefender;
}

} // namespace ac
