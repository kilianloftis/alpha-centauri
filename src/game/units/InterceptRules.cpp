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
    // The base the building charge came from, when the candidate was collected from a specific
    // base's effects (the ThisBase lane). Null for FactionGlobal / AllOwnerBases charges, which
    // are not tied to one base. Destroy-on-fail used to re-derive the base with
    // FindBaseWithBuilding, which returns the *first* base owning that id — so with the same
    // building in two bases the wrong copy was destroyed and the deploy cleared against the
    // wrong inventory.
    BaseManager* pBaseSource = nullptr;
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
                               Unit* pUnitSource,
                               BaseManager* pBaseSource)
{
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (std::find(allowedScopes.begin(), allowedScopes.end(), rEffect.config->scope)
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
        candidate.pBaseSource = pBaseSource;
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
    // The intercepting side is the defender, and the live attacker is available — so say so.
    // Marking the role Attacker and leaving pAttacker null meant an IsDefending condition on
    // an intercept effect never matched and AttackerIsEmbarked was always false, silently, even
    // though UnitFilterSatisfied right below already has the attacker.
    EffectContext_t ctx{&rDefender.GetTile(), CombatRole_t::Defender};
    ctx.pAttacker = &rAttacker;
    std::vector<InterceptCandidate_t> candidates;

    // Faction-wide charges are not tied to one base, so there is no originating base to carry.
    AppendMatchingIntercepts_(candidates, rDefFaction.GetActiveEffects().effects, rAttacker, ctx,
                              {EffectScope_t::FactionGlobal, EffectScope_t::AllOwnerBases},
                              InterceptDeployKind_t::Building, nullptr, nullptr);

    if (BaseManager* pBase =
            rGameState.FindBaseAt(rDefender.GetTile().GetX(), rDefender.GetTile().GetY());
        pBase && &pBase->GetFaction() == &rDefFaction)
    {
        // A ThisBase charge belongs to *this* base; carry it so destroy-on-fail hits the copy
        // that actually fired rather than whichever base FindBaseWithBuilding happens to return.
        AppendMatchingIntercepts_(candidates, pBase->CollectBuildingEffects(), rAttacker, ctx,
                                  {EffectScope_t::ThisBase}, InterceptDeployKind_t::Building,
                                  nullptr, pBase);
    }

    AppendMatchingIntercepts_(candidates, rDefender.GetDesign().CollectEffects(), rAttacker, ctx,
                              {EffectScope_t::ThisUnit}, InterceptDeployKind_t::Unit, &rDefender,
                              nullptr);

    // TODO: a ThisTile source has no deploy ledger to charge, so it ignores cooldownTurns and
    // may attempt on every attack. Honouring the configured cooldown needs a per-tile (or
    // per-improvement) deploy record — the rule for that is not defined yet.
    // TODO: this is the only CollectAreaEffects consumer that ignores ActiveEffect_t::ownerFaction,
    // so an intercept aura projected by a hostile unit or an enemy-owned territory improvement
    // still fires for the defender. Gating on AppliesForFaction(effect, defender faction) is the
    // obvious fix, but whether a neutral/allied tile source may intercept is not a settled rule.
    AppendMatchingIntercepts_(candidates, rTileEffects.CollectAreaEffects(rDefender.GetTile()),
                              rAttacker, ctx, {EffectScope_t::ThisTile},
                              InterceptDeployKind_t::None, nullptr, nullptr);

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
    {
        // DeployBuilding appends a deploy record that CountReadyBuildings subtracts, so the
        // faction's own ready count is the running tally across candidates — no local memo.
        if (rDefFaction.CountReadyBuildings(rCandidate.sourceId, missionYear) <= 0)
        {
            return false;
        }
        // Attribute the deploy to the same base destroy-on-fail will charge. These two must
        // agree: NotifyBuildingDestroyed only erases a record whose baseId matches, so keying
        // the deploy on FindBaseWithBuilding (the *first* base owning the id) while destroying
        // through pBaseSource left an unerasable record — the surviving copy in another base
        // stayed suppressed for the whole cooldown.
        //
        // FindBaseWithBuilding remains the fallback for FactionGlobal / AllOwnerBases charges,
        // which belong to no single base; there, best-effort attribution is all the model
        // supports (see Faction::DeployBuilding).
        const BaseManager* pBase = rCandidate.pBaseSource
                                       ? rCandidate.pBaseSource
                                       : rDefFaction.FindBaseWithBuilding(rCandidate.sourceId);
        const BaseId_t baseId = pBase ? pBase->GetBaseId() : BaseId_t{};
        rDefFaction.DeployBuilding(
            baseId, rCandidate.sourceId,
            ReadyYearAfterDeploy(missionYear, rIntercept.cooldownTurns));
        return true;
    }
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
void MaybeDestroyInterceptSourceOnFail_(GameState& rGameState, Faction& rDefFaction,
                                        InterceptCandidate_t& rCandidate,
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
        // Prefer the base the charge actually came from. FindBaseWithBuilding is only the
        // fallback for faction-wide charges, which belong to no single base — for those,
        // destroying "a" copy is the best the model supports.
        BaseManager* pBase = rCandidate.pBaseSource
                                 ? rCandidate.pBaseSource
                                 : rDefFaction.FindBaseWithBuilding(rCandidate.sourceId);
        if (!pBase)
        {
            throw std::logic_error(
                "MaybeDestroyInterceptSourceOnFail_: faction owns no base holding building '"
                + rCandidate.sourceId + "'");
        }
        // Read the config before the copy is gone.
        const BuildingConfig_t* pConfig =
            rDefFaction.FindOwnedBuildingConfig(rCandidate.sourceId);
        pBase->GetBuildingManager().DestroyBuilding(rCandidate.sourceId);
        rDefFaction.NotifyBuildingDestroyed(pBase->GetBaseId(), rCandidate.sourceId);
        // Same tombstone rule as raze and ASAT: a destroyed secret project stays destroyed.
        if (pConfig && pConfig->bIsSecretProject)
        {
            rGameState.MarkSecretProjectDestroyed(rCandidate.sourceId);
        }
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
            MaybeDestroyInterceptSourceOnFail_(rGameState, rDefFaction, rCandidate, rRng);
            continue;
        }
        return ResolveInterceptKill_(rAttacker, rDefender);
    }
    return std::nullopt;
}

} // namespace ac
