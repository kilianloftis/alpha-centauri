#include "game/combat/AttackRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/combat/CombatRules.h"
#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/InteractionResolve.h"
#include "game/effects/TileEffectsContext.h"
#include "game/visibility/VisibilityRules.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementConstants.h"
#include "game/movement/MovementRules.h"
#include "game/movement/TransportRules.h"
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

bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const InteractionGridsConfig_t& rGrids = rGame.GetGameData().interactionGrids;

    // No attacking a tile the unit could not enter...
    if (!CanPhysicallyEnterTile(rAttacker, rTargetTile))
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

Unit* SelectDefender(const Unit& rAttacker, const std::vector<Unit*>& rCandidates)
{
    Unit* pBest = nullptr;
    double bestChance = 0.0;
    for (Unit* pCandidate : rCandidates)
    {
        const double chance = DefenderWinChance(rAttacker, *pCandidate);
        if (pBest == nullptr || chance > bestChance)
        {
            pBest = pCandidate;
            bestChance = chance;
        }
    }
    return pBest;
}

Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();

    std::vector<Unit*> hostiles;
    AppendHostiles_(rObserver, rTile, rWorldMap, hostiles);
    std::vector<Unit*> visible;
    for (Unit* pUnit : hostiles)
    {
        if (IsUnitVisibleTo(rObserver.GetFaction(), *pUnit))
        {
            visible.push_back(pUnit);
        }
    }
    return SelectDefender(rObserver, visible);
}

Unit* FindAttackableHostileOnTile(const Unit& rAttacker, const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const InteractionGridsConfig_t& rGrids = rGame.GetGameData().interactionGrids;

    if (rAttacker.GetMoveFragmentsRemaining() <= 0)
    {
        return nullptr;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile, rWorldMap.GetWidth()))
    {
        return nullptr;
    }
    if (!CanAttackTile(rAttacker, rTargetTile))
    {
        return nullptr;
    }
    Unit* pDefender = FindVisibleHostileOnTile(rAttacker, rTargetTile);
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

bool IsWithinBombardRange(const Unit& rAttacker, const Tile& rTargetTile)
{
    if (!CanBombard(rAttacker))
    {
        return false;
    }
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const int distance = ChebyshevDistance(rAttacker.GetTile(), rTargetTile, rWorldMap.GetWidth());
    const int range = rAttacker.GetStat(StatId_t::BombardRange);
    return distance >= 1 && distance <= range;
}

Unit* ArtilleryDuelTarget(const Unit& rAttacker, const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const InteractionGridsConfig_t& rGrids = rGame.GetGameData().interactionGrids;

    std::vector<Unit*> hostiles;
    AppendHostiles_(rAttacker, rTargetTile, rWorldMap, hostiles);

    std::vector<Unit*> duellists;
    for (Unit* pHostile : hostiles)
    {
        if (pHostile->GetFlag(RuleFlagId_t::Bombard)
            && PassesAttackUnit_(rAttacker, *pHostile, rTargetTile, rWorldMap, rGrids))
        {
            duellists.push_back(pHostile);
        }
    }
    return SelectDefender(rAttacker, duellists);
}

std::vector<Unit*> BombardStrikeTargets(const Unit& rAttacker, const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const InteractionGridsConfig_t& rGrids = rGame.GetGameData().interactionGrids;

    std::vector<Unit*> hostiles;
    AppendHostiles_(rAttacker, rTargetTile, rWorldMap, hostiles);

    std::vector<Unit*> targets;
    for (Unit* pHostile : hostiles)
    {
        if (pHostile->GetFlag(RuleFlagId_t::Bombard))
        {
            return {};
        }
        if (PassesAttackUnit_(rAttacker, *pHostile, rTargetTile, rWorldMap, rGrids))
        {
            targets.push_back(pHostile);
        }
    }
    return targets;
}

std::vector<std::string> BombardImprovementTargets(const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    if (!rWorldMap.GetUnitsOnTile(rTargetTile).empty()
        || !rWorldMap.GetCargoOnTile(rTargetTile).empty())
    {
        return {};
    }
    std::vector<std::string> ids;
    for (const ImprovementConfig_t* pImprovement : rTargetTile.GetImprovements())
    {
        if (pImprovement->id != ImprovementIds::k_Base)
        {
            ids.push_back(pImprovement->id);
        }
    }
    return ids;
}

int AttackMoveFragments(const Unit& rAttacker, AttackKind_t kind)
{
    if (kind == AttackKind_t::Bombard || rAttacker.GetFlag(RuleFlagId_t::AttackingEndsTurn))
    {
        return rAttacker.GetMoveFragmentsRemaining();
    }
    return MovementConstants_t::k_moveFragmentsPerPoint;
}

} // namespace ac
