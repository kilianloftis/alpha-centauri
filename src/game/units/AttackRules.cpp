#include "game/units/AttackRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/InteractionResolve.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

namespace ac
{

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
    const bool bBaseTile = rTile.HasImprovement(ImprovementIds::k_Base);
    Unit* pEmbarkedInBase = nullptr;
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (!pUnit || pUnit->GetFaction().GetFactionId() == observerId
            || !IsUnitVisibleTo(rObserverFaction, *pUnit, rTileEffects))
        {
            continue;
        }
        if (!pUnit->IsEmbarked())
        {
            return pUnit;
        }
        // Embarked cargo defends only in a base; prefer a non-embarked hostile above.
        if (bBaseTile && !pEmbarkedInBase)
        {
            pEmbarkedInBase = pUnit;
        }
    }
    return pEmbarkedInBase;
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
    // A grounded aircraft is attackable by anything: sitting on a pad (base, airbase,
    // friendly carrier deck) is what takes it out of its own domain's protection, so that
    // exemption derives from the tile's RefuelsAir rather than from the grid.
    if (ResolveInteractionCell(rGrids, q, &rAttacker, ctx) == InteractionCell_t::Deny
        && !TileProvidesFlag(rTargetTile, RuleFlagId_t::RefuelsAir, rWorldMap,
                             pDefender->GetFaction().GetFactionId()))
    {
        return nullptr;
    }
    return pDefender;
}

} // namespace ac
