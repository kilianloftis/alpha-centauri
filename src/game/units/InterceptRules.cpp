#include "game/units/InterceptRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/DeployCooldown.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "lib/RandomRoll.h"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

// Which deploy ledger an intercept source charges its cooldown against. Fixed per source
// lane by CollectInterceptCandidates_, not re-derived from the effect's scope.
enum class InterceptDeployKind_t
{
    None,
    Building,
    Unit,
};

struct InterceptCandidate_t
{
    const InterceptAttemptEffect_t* pIntercept = nullptr;
    // Ledger used for cooldown (None when the effect opts out or the lane has no ledger).
    InterceptDeployKind_t deployKind = InterceptDeployKind_t::None;
    // Physical source kind for destroy-on-fail (not downgraded when cooldown is opted out).
    InterceptDeployKind_t sourceKind = InterceptDeployKind_t::None;
    // ActiveEffect_t::sourceId of the granting source — a BuildingId_t only on the Building
    // lane; a component or tile-feature id otherwise.
    std::string sourceId;
    Unit* pUnitSource = nullptr;
};

// sourceKind is the calling lane's physical source. deployKind matches it unless the effect
// opts out of cooldowns (cooldownTurns < 0), in which case deploy is None so it may attempt
// on every attack.
void AppendMatchingIntercepts_(std::vector<InterceptCandidate_t>& rOut,
                               const std::vector<ActiveEffect_t>& rEffects,
                               const Unit& rAttacker,
                               const EffectContext_t& rCtx,
                               std::initializer_list<EffectScope_t> allowedScopes,
                               InterceptDeployKind_t sourceKind,
                               Unit* pUnitSource)
{
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (!rEffect.config
            || std::find(allowedScopes.begin(), allowedScopes.end(), rEffect.config->scope)
                   == allowedScopes.end())
        {
            continue;
        }
        const auto* pIntercept =
            std::get_if<InterceptAttemptEffect_t>(&rEffect.config->effect);
        if (!pIntercept
            || !UnitFilterSatisfied(*rEffect.config, rAttacker)
            || !ConditionSatisfied(*rEffect.config, rCtx, rEffect.originBase))
        {
            continue;
        }
        InterceptCandidate_t candidate;
        candidate.pIntercept = pIntercept;
        candidate.sourceId = rEffect.sourceId;
        candidate.pUnitSource = pUnitSource;
        candidate.sourceKind = sourceKind;
        candidate.deployKind =
            pIntercept->cooldownTurns < 0 ? InterceptDeployKind_t::None : sourceKind;
        rOut.push_back(candidate);
    }
}

std::vector<InterceptCandidate_t> CollectInterceptCandidates_(GameState& rGameState,
                                                              Unit& rAttacker,
                                                              Unit& rDefender,
                                                              TileEffectsContext& rTileEffects)
{
    Faction& rDefFaction = rDefender.GetFaction();
    const EffectContext_t ctx{&rDefender.GetTile(), CombatRole_t::Attacker};
    std::vector<InterceptCandidate_t> candidates;

    AppendMatchingIntercepts_(candidates, rDefFaction.GetActiveEffects().effects, rAttacker, ctx,
                              {EffectScope_t::FactionGlobal, EffectScope_t::AllOwnerBases},
                              InterceptDeployKind_t::Building, nullptr);

    if (const BaseManager* pBase =
            rGameState.FindBaseAt(rDefender.GetTile().GetX(), rDefender.GetTile().GetY());
        pBase && &pBase->GetFaction() == &rDefFaction)
    {
        AppendMatchingIntercepts_(candidates, pBase->CollectBuildingEffects(), rAttacker, ctx,
                                  {EffectScope_t::ThisBase}, InterceptDeployKind_t::Building,
                                  nullptr);
    }

    AppendMatchingIntercepts_(candidates, rDefender.GetDesign().CollectEffects(), rAttacker, ctx,
                              {EffectScope_t::ThisUnit}, InterceptDeployKind_t::Unit, &rDefender);

    // TODO: a ThisTile source has no deploy ledger to charge, so it ignores cooldownTurns and
    // may attempt on every attack. Honouring the configured cooldown needs a per-tile (or
    // per-improvement) deploy record — the rule for that is not defined yet.
    AppendMatchingIntercepts_(candidates, rTileEffects.CollectAreaEffects(rDefender.GetTile()),
                              rAttacker, ctx, {EffectScope_t::ThisTile},
                              InterceptDeployKind_t::None, nullptr);

    return candidates;
}

// Returns false when the source cannot act (already deployed / no ready charge).
bool TryDeployInterceptSource_(Faction& rDefFaction,
                               InterceptCandidate_t& rCandidate,
                               int missionYear)
{
    const InterceptAttemptEffect_t& rIntercept = *rCandidate.pIntercept;
    switch (rCandidate.deployKind)
    {
    case InterceptDeployKind_t::None:
        return true;
    case InterceptDeployKind_t::Unit:
        if (!rCandidate.pUnitSource->IsInterceptReady(missionYear))
        {
            return false;
        }
        rCandidate.pUnitSource->DeployIntercept(
            ReadyYearAfterDeploy(missionYear, rIntercept.cooldownTurns));
        return true;
    case InterceptDeployKind_t::Building:
        // DeployBuilding appends a deploy record that CountReadyBuildings subtracts, so the
        // faction's own ready count is the running tally across candidates — no local memo.
        if (rDefFaction.CountReadyBuildings(rCandidate.sourceId, missionYear) <= 0)
        {
            return false;
        }
        rDefFaction.DeployBuilding(
            rCandidate.sourceId, ReadyYearAfterDeploy(missionYear, rIntercept.cooldownTurns));
        return true;
    }
    return false;
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

// On a failed intercept roll, optionally destroy the intercepting source (building or unit).
void MaybeDestroyInterceptSourceOnFail_(Faction& rDefFaction, InterceptCandidate_t& rCandidate,
                                        std::mt19937& rRng)
{
    if (!RollPercent(rCandidate.pIntercept->chanceOfDestructionOnFail, rRng))
    {
        return;
    }
    switch (rCandidate.sourceKind)
    {
    case InterceptDeployKind_t::None:
        // ThisTile (and other non-instance) sources have nothing to destroy.
        return;
    case InterceptDeployKind_t::Building:
    {
        BaseManager* pBase = rDefFaction.FindBaseWithBuilding(rCandidate.sourceId);
        if (!pBase)
        {
            throw std::logic_error(
                "MaybeDestroyInterceptSourceOnFail_: faction owns no base holding building '"
                + rCandidate.sourceId + "'");
        }
        pBase->GetBuildingManager().DestroyBuilding(rCandidate.sourceId);
        rDefFaction.NotifyBuildingDestroyed(rCandidate.sourceId);
        return;
    }
    case InterceptDeployKind_t::Unit:
        if (rCandidate.pUnitSource)
        {
            rDefFaction.GetUnitManager().DestroyUnit(*rCandidate.pUnitSource);
        }
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
    Faction& rDefFaction = rDefender.GetFaction();
    const int year = rGameState.GetMissionYear();
    std::vector<InterceptCandidate_t> candidates =
        CollectInterceptCandidates_(rGameState, rAttacker, rDefender, rTileEffects);

    for (InterceptCandidate_t& rCandidate : candidates)
    {
        if (!TryDeployInterceptSource_(rDefFaction, rCandidate, year))
        {
            continue;
        }
        if (!RollPercent(rCandidate.pIntercept->chance, rRng))
        {
            MaybeDestroyInterceptSourceOnFail_(rDefFaction, rCandidate, rRng);
            continue;
        }
        return ResolveInterceptKill_(rAttacker, rDefender);
    }
    return std::nullopt;
}

} // namespace ac
