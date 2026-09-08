#include "game/units/AttackRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
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

bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile, const WorldMap& rWorldMap)
{
    // Every domain: no attacking a tile the unit could not enter.
    if (!CanEnterTile(rAttacker, rTargetTile, rWorldMap))
    {
        return false;
    }

    // Channel-crossing Permission(AttackTile) is a land rule (embarked cargo, shore <-> sea).
    if (rAttacker.GetDomain() != UnitDomain_t::Land)
    {
        return true;
    }

    const bool bChannelCrossing = rAttacker.IsEmbarked()
        || rAttacker.GetTile().IsWater() != rTargetTile.IsWater();
    if (!bChannelCrossing)
    {
        return true;
    }

    EffectContext_t ctx;
    ctx.targetTile = &rTargetTile;
    ctx.pAttacker = &rAttacker;
    return HasPermission(rAttacker, PermissionId_t::AttackTile, ctx);
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
    if (rAttacker.GetMoveFragmentsRemaining() <= 0)
    {
        return nullptr;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile, rWorldMap.GetWidth()))
    {
        return nullptr;
    }
    if (!CanAttackTile(rAttacker, rTargetTile, rWorldMap))
    {
        return nullptr;
    }
    Unit* pDefender =
        FindVisibleHostileOnTile(rAttacker, rTargetTile, rWorldMap, rTileEffects);
    if (!pDefender)
    {
        return nullptr;
    }

    // Air / Orbital hostiles require Permission(AttackDomain) covering that domain, unless
    // the tile provides RefuelsAir (base, airbase, friendly carrier deck).
    const UnitDomain_t defenderDomain = pDefender->GetDomain();
    if (defenderDomain == UnitDomain_t::Air || defenderDomain == UnitDomain_t::Orbital)
    {
        if (!TileProvidesFlag(rTargetTile, RuleFlagId_t::RefuelsAir, rWorldMap,
                              pDefender->GetFaction().GetFactionId()))
        {
            EffectContext_t ctx;
            ctx.pAttacker = &rAttacker;
            ctx.hostileDomain = defenderDomain;
            if (!HasPermission(rAttacker, PermissionId_t::AttackDomain, ctx))
            {
                return nullptr;
            }
        }
    }
    return pDefender;
}

} // namespace ac
