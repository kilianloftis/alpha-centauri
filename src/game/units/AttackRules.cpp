#include "game/units/AttackRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/InteractionResolve.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/ImprovementConfigParser.h"
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

bool PassesAttackUnit_(const Unit& rAttacker, const Unit& rDefender, const Tile& rTargetTile,
                       const WorldMap& rWorldMap, const InteractionGridsConfig_t& rGrids)
{
    InteractionQuery_t q;
    q.grid = InteractionGridId_t::AttackUnit;
    q.actorDomain = rAttacker.GetDomain();
    q.targetDomain = rDefender.GetDomain();
    EffectContext_t ctx;
    ctx.pAttacker = &rAttacker;
    ctx.targetTile = &rTargetTile;
    return ResolveInteractionCell(rGrids, q, &rAttacker, ctx) != InteractionCell_t::Deny
           || DefenderIsResting_(rDefender, rTargetTile, rWorldMap);
}

// Surface hostiles, then embarked cargo when the tile has a base.
void AppendHostiles_(const Unit& rObserver, const Tile& rTile, const WorldMap& rWorldMap,
                     std::vector<Unit*>& rOut)
{
    const FactionId_t observerId = rObserver.GetFaction().GetFactionId();
    auto consider = [&](Unit* pUnit)
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != observerId)
        {
            rOut.push_back(pUnit);
        }
    };
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        consider(pUnit);
    }
    if (rTile.HasImprovement(ImprovementIds::k_Base))
    {
        for (Unit* pUnit : rWorldMap.GetCargoOnTile(rTile))
        {
            consider(pUnit);
        }
    }
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
    std::vector<Unit*> hostiles;
    AppendHostiles_(rObserver, rTile, rWorldMap, hostiles);
    for (Unit* pUnit : hostiles)
    {
        if (IsUnitVisibleTo(rObserver.GetFaction(), *pUnit, rTileEffects))
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

    if (!PassesAttackUnit_(rAttacker, *pDefender, rTargetTile, rWorldMap, rGrids))
    {
        return nullptr;
    }
    return pDefender;
}

bool CanBombard(const Unit& rAttacker)
{
    return rAttacker.GetFlag(RuleFlagId_t::Bombard)
           && rAttacker.GetMoveFragmentsRemaining() > 0;
}

bool IsWithinBombardRange(const Unit& rAttacker, const Tile& rTargetTile,
                          const WorldMap& rWorldMap)
{
    if (!CanBombard(rAttacker))
    {
        return false;
    }
    const int distance = ChebyshevDistance(rAttacker.GetTile(), rTargetTile, rWorldMap.GetWidth());
    const int range = rAttacker.GetStat(StatId_t::BombardRange);
    return distance >= 1 && distance <= range;
}

BombardTargeting_t CollectBombardTargets(const Unit& rAttacker, const Tile& rTargetTile,
                                         const WorldMap& rWorldMap,
                                         const TileEffectsContext& rTileEffects)
{
    const InteractionGridsConfig_t& rGrids = rTileEffects.GetInteractionGrids();
    std::vector<Unit*> hostiles;
    AppendHostiles_(rAttacker, rTargetTile, rWorldMap, hostiles);

    BombardTargeting_t result;
    bool bAnyBombard = false;
    for (Unit* pHostile : hostiles)
    {
        const bool bLegal = PassesAttackUnit_(rAttacker, *pHostile, rTargetTile, rWorldMap, rGrids);
        if (pHostile->GetFlag(RuleFlagId_t::Bombard))
        {
            bAnyBombard = true;
            if (result.pDuelTarget == nullptr && bLegal)
            {
                result.pDuelTarget = pHostile;
            }
        }
        else if (bLegal)
        {
            result.strikeTargets.push_back(pHostile);
        }
    }
    if (bAnyBombard)
    {
        result.strikeTargets.clear();
        result.bBombardPresentButIllegal = result.pDuelTarget == nullptr;
    }
    return result;
}

bool TileHasUnits(const Tile& rTile, const WorldMap& rWorldMap)
{
    return !rWorldMap.GetUnitsOnTile(rTile).empty()
           || !rWorldMap.GetCargoOnTile(rTile).empty();
}

std::vector<std::string> NonBaseImprovementIds(const Tile& rTile)
{
    std::vector<std::string> ids;
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement && pImprovement->id != ImprovementIds::k_Base)
        {
            ids.push_back(pImprovement->id);
        }
    }
    return ids;
}

} // namespace ac
