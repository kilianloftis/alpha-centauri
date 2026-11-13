#include "game/combat/InterceptMutations.h"

#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/combat/InterceptRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/DeployCooldown.h"
#include "game/combat/TileInterceptDeployLedger.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"
#include "lib/RandomRoll.h"

#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

Faction& Ledger_(GameState& rGameState, const InterceptBuildingIdentity_t& rBuilding)
{
    Faction* pLedger = rGameState.FindFaction(rBuilding.ledgerFactionId);
    if (pLedger == nullptr)
    {
        throw std::logic_error("Intercept source bills a faction that is not in the game");
    }
    return *pLedger;
}

Unit& InterceptUnit_(GameState& rGameState, const InterceptUnitIdentity_t& rUnitId)
{
    for (Faction& rFaction : rGameState.Factions())
    {
        for (Unit& rUnit : rFaction.GetUnitManager().Units())
        {
            if (rUnit.GetUnitId() == rUnitId.unitId)
            {
                return rUnit;
            }
        }
    }
    throw std::logic_error("Intercept source names a unit that is not in the game");
}

void DeployInterceptSource_(GameState& rGameState, const InterceptSource_t& rSource,
                            int missionYear)
{
    const int readyYear = ReadyYearAfterDeploy(missionYear, rSource.pIntercept->cooldownTurns);
    switch (rSource.deployKind)
    {
    case InterceptSourceKind_t::None:
        return;
    case InterceptSourceKind_t::Unit:
    {
        const auto& rUnit = std::get<InterceptUnitIdentity_t>(rSource.identity);
        InterceptUnit_(rGameState, rUnit).Latches().DeployIntercept(readyYear);
        return;
    }
    case InterceptSourceKind_t::Building:
    {
        const auto& rBuilding = std::get<InterceptBuildingIdentity_t>(rSource.identity);
        const std::optional<BaseId_t> baseId = InterceptChargeBase(rSource);
        Ledger_(rGameState, rBuilding)
            .GetBuildingDeploys()
            .Deploy(baseId.value_or(BaseId_t{}), rBuilding.sourceId, readyYear);
        return;
    }
    case InterceptSourceKind_t::Tile:
    {
        const auto& rTile = std::get<InterceptTileIdentity_t>(rSource.identity);
        rGameState.GetTileInterceptDeploys().Deploy(rTile.hostTile, rTile.sourceId, readyYear);
        return;
    }
    }
}

CombatResult_t ResolveInterceptKill_(Unit& rAttacker, Unit& rDefender)
{
    CombatResult_t result;
    result.attackerId = rAttacker.GetUnitId();
    result.defenderId = rDefender.GetUnitId();
    result.victor = CombatSide_t::Defender;
    result.bAttackerDestroyed = true;
    rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
    return result;
}

// No fight took place, so the attacker holds the field without having won one.
CombatResult_t LoseDefenderToItsIntercept_(Unit& rAttacker, Unit& rDefender)
{
    CombatResult_t result;
    result.attackerId = rAttacker.GetUnitId();
    result.defenderId = rDefender.GetUnitId();
    result.victor = CombatSide_t::Attacker;
    result.bDefenderDestroyed = true;
    rDefender.GetFaction().GetUnitManager().DestroyUnit(rDefender);
    return result;
}

// Losing the defender's own design loses the defender, which ends the attack.
std::optional<CombatResult_t> LoseInterceptSource_(GameState& rGameState, Unit& rAttacker,
                                                   Unit& rDefender,
                                                   const InterceptSource_t& rSource)
{
    switch (InterceptPhysicalKind(rSource))
    {
    case InterceptSourceKind_t::None:
        return std::nullopt;
    case InterceptSourceKind_t::Building:
    {
        const auto& rBuilding = std::get<InterceptBuildingIdentity_t>(rSource.identity);
        const std::optional<BaseId_t> baseId = InterceptChargeBase(rSource);
        BaseManager* pBase =
            baseId ? Ledger_(rGameState, rBuilding).FindBase(*baseId) : nullptr;
        if (pBase == nullptr)
        {
            throw std::logic_error("DestroyInterceptSource_: no base holds intercept source '"
                                   + rBuilding.sourceId + "'");
        }
        if (!pBase->GetBuildingManager().HasBuilding(rBuilding.sourceId))
        {
            throw std::logic_error("DestroyInterceptSource_: base does not hold intercept source '"
                                   + rBuilding.sourceId + "'");
        }
        pBase->GetBuildingManager().DestroyBuilding(rBuilding.sourceId);
        return std::nullopt;
    }
    case InterceptSourceKind_t::Unit:
    {
        const auto& rUnitId = std::get<InterceptUnitIdentity_t>(rSource.identity);
        Unit& rLost = InterceptUnit_(rGameState, rUnitId);
        if (&rLost == &rDefender)
        {
            return LoseDefenderToItsIntercept_(rAttacker, rDefender);
        }
        rLost.GetFaction().GetUnitManager().DestroyUnit(rLost);
        return std::nullopt;
    }
    case InterceptSourceKind_t::Tile:
    {
        const auto& rTileId = std::get<InterceptTileIdentity_t>(rSource.identity);
        Tile* pHost = rGameState.GetWorldMap().GetTile(rTileId.hostTile);
        if (pHost == nullptr)
        {
            throw std::logic_error("Tile intercept lose names a host tile that is not on the map");
        }
        if (!pHost->HasFeature(rTileId.sourceId))
        {
            throw std::logic_error("DestroyInterceptSource_: host tile does not hold intercept "
                                   "source '"
                                   + rTileId.sourceId + "'");
        }
        rGameState.GetTileEffects().RemoveOccupantWithEffects(*pHost, rTileId.sourceId);
        return std::nullopt;
    }
    }
    throw std::logic_error("LoseInterceptSource_: unknown intercept source kind");
}

} // namespace

std::optional<CombatResult_t> TryInterceptAttack(GameState& rGameState,
                                                 Unit& rAttacker,
                                                 Unit& rDefender,
                                                 std::mt19937& rRng)
{
    const int year = rGameState.GetMissionYear();
    const std::vector<InterceptSource_t> sources = CollectInterceptSources(rAttacker, rDefender);

    for (const InterceptSource_t& rSource : sources)
    {
        if (!InterceptSourceReady(rSource, year))
        {
            continue;
        }
        DeployInterceptSource_(rGameState, rSource, year);
        if (RollPercent(rSource.pIntercept->chance, rRng))
        {
            return ResolveInterceptKill_(rAttacker, rDefender);
        }
        if (!RollPercent(rSource.pIntercept->chanceOfDestructionOnFail, rRng))
        {
            continue;
        }
        if (std::optional<CombatResult_t> lost =
                LoseInterceptSource_(rGameState, rAttacker, rDefender, rSource))
        {
            return lost;
        }
    }
    return std::nullopt;
}

} // namespace ac
