#include "game/combat/InterceptMutations.h"

#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/combat/InterceptRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/DeployCooldown.h"
#include "game/faction/Faction.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"
#include "lib/RandomRoll.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

Faction& Ledger_(GameState& rGameState, const InterceptSource_t& rSource)
{
    Faction* pLedger = rGameState.FindFaction(rSource.ledgerFactionId);
    if (pLedger == nullptr)
    {
        throw std::logic_error("Intercept source bills a faction that is not in the game");
    }
    return *pLedger;
}

void DeployInterceptSource_(GameState& rGameState, Unit& rDefender,
                            const InterceptSource_t& rSource, int missionYear)
{
    const int readyYear = ReadyYearAfterDeploy(missionYear, rSource.pIntercept->cooldownTurns);
    switch (rSource.deployKind)
    {
    case InterceptSourceKind_t::None:
        return;
    case InterceptSourceKind_t::Unit:
        rDefender.DeployIntercept(readyYear);
        return;
    case InterceptSourceKind_t::Building:
    {
        const std::optional<BaseId_t> baseId = InterceptChargeBase(rSource);
        Ledger_(rGameState, rSource)
            .DeployBuilding(baseId.value_or(BaseId_t{}), rSource.sourceId, readyYear);
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

void DestroyInterceptSource_(GameState& rGameState, Unit& rDefender,
                             const InterceptSource_t& rSource)
{
    switch (rSource.sourceKind)
    {
    case InterceptSourceKind_t::None:
        return;
    case InterceptSourceKind_t::Building:
    {
        const std::optional<BaseId_t> baseId = InterceptChargeBase(rSource);
        BaseManager* pBase =
            baseId ? Ledger_(rGameState, rSource).FindBase(*baseId) : nullptr;
        if (pBase == nullptr)
        {
            throw std::logic_error("DestroyInterceptSource_: no base holds intercept source '"
                                   + rSource.sourceId + "'");
        }
        if (!pBase->GetBuildingManager().HasBuilding(rSource.sourceId))
        {
            throw std::logic_error("DestroyInterceptSource_: base does not hold intercept source '"
                                   + rSource.sourceId + "'");
        }
        pBase->GetBuildingManager().DestroyBuilding(rSource.sourceId);
        return;
    }
    case InterceptSourceKind_t::Unit:
        rDefender.GetFaction().GetUnitManager().DestroyUnit(rDefender);
        return;
    }
}

} // namespace

std::optional<CombatResult_t> TryInterceptAttack(GameState& rGameState,
                                                 Unit& rAttacker,
                                                 Unit& rDefender,
                                                 TileEffectsContext& rTileEffects,
                                                 std::mt19937& rRng)
{
    const int year = rGameState.GetMissionYear();
    const std::vector<InterceptSource_t> sources =
        CollectInterceptSources(rAttacker, rDefender, rTileEffects);

    for (const InterceptSource_t& rSource : sources)
    {
        if (!InterceptSourceReady(rSource, rDefender, year))
        {
            continue;
        }
        DeployInterceptSource_(rGameState, rDefender, rSource, year);
        if (RollPercent(rSource.pIntercept->chance, rRng))
        {
            return ResolveInterceptKill_(rAttacker, rDefender);
        }
        if (RollPercent(rSource.pIntercept->chanceOfDestructionOnFail, rRng))
        {
            DestroyInterceptSource_(rGameState, rDefender, rSource);
        }
    }
    return std::nullopt;
}

} // namespace ac
