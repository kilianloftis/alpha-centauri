#include "game/combat/InterceptRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/combat/TileInterceptDeployLedger.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <variant>

namespace ac
{

namespace
{

InterceptSource_t MakeSource_(const InterceptEffect_t& rIntercept,
                              InterceptSourceKind_t physicalKind, InterceptIdentity_t identity)
{
    InterceptSource_t source;
    source.pIntercept = &rIntercept;
    source.deployKind =
        rIntercept.cooldownTurns < 0 ? InterceptSourceKind_t::None : physicalKind;
    source.identity = std::move(identity);
    return source;
}

template <typename MakeIdentity>
void AppendMatchingIntercepts_(std::vector<InterceptSource_t>& rOut,
                               const std::vector<ActiveEffect_t>& rEffects,
                               const EffectContext_t& rCtx,
                               std::initializer_list<EffectScope_t> allowedScopes,
                               InterceptSourceKind_t physicalKind, MakeIdentity makeIdentity)
{
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (std::find(allowedScopes.begin(), allowedScopes.end(), rEffect.config->scope)
                == allowedScopes.end())
        {
            continue;
        }
        const auto* pIntercept = std::get_if<InterceptEffect_t>(&rEffect.config->effect);
        if (!pIntercept
            || !ConditionSatisfied(*rEffect.config, ActiveEffectContext(rEffect, rCtx),
                                  rEffect.originBase))
        {
            continue;
        }
        rOut.push_back(MakeSource_(*pIntercept, physicalKind, makeIdentity(rEffect)));
    }
}

void AppendTileIntercepts_(std::vector<InterceptSource_t>& rOut,
                           const std::vector<ActiveEffect_t>& rEffects,
                           const EffectContext_t& rCtx)
{
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (rEffect.config->scope != EffectScope_t::ThisTile)
        {
            continue;
        }
        const auto* pIntercept = std::get_if<InterceptEffect_t>(&rEffect.config->effect);
        if (!pIntercept
            || !ConditionSatisfied(*rEffect.config, ActiveEffectContext(rEffect, rCtx),
                                  rEffect.originBase))
        {
            continue;
        }
        if (rEffect.projectingUnitId.has_value())
        {
            rOut.push_back(MakeSource_(
                *pIntercept, InterceptSourceKind_t::Unit,
                InterceptUnitIdentity_t{rEffect.sourceId, *rEffect.projectingUnitId}));
            continue;
        }
        if (!rEffect.hostTile.has_value())
        {
            throw std::logic_error("ThisTile Intercept has no host tile");
        }
        rOut.push_back(MakeSource_(*pIntercept, InterceptSourceKind_t::Tile,
                                   InterceptTileIdentity_t{rEffect.sourceId, *rEffect.hostTile}));
    }
}

const Faction& Ledger_(const InterceptBuildingIdentity_t& rBuilding)
{
    const Faction* pLedger = CurrentGameState().FindFaction(rBuilding.ledgerFactionId);
    if (pLedger == nullptr)
    {
        throw std::logic_error("Intercept source bills a faction that is not in the game");
    }
    return *pLedger;
}

const Unit& InterceptUnit_(const InterceptUnitIdentity_t& rUnitId)
{
    for (const Faction& rFaction : CurrentGameState().Factions())
    {
        for (const Unit& rUnit : rFaction.GetUnitManager().Units())
        {
            if (rUnit.GetUnitId() == rUnitId.unitId)
            {
                return rUnit;
            }
        }
    }
    throw std::logic_error("Intercept source names a unit that is not in the game");
}

} // namespace

std::vector<InterceptSource_t> CollectInterceptSources(const Unit& rAttacker,
                                                       const Unit& rDefender)
{
    const GameState& rState = CurrentGameState();
    const TileEffectsContext& rTileEffects = rState.GetTileEffects();
    const Faction& rDefFaction = rDefender.GetFaction();
    const FactionId_t defenderId = rDefFaction.GetFactionId();
    const UnitId_t defenderUnitId = rDefender.GetUnitId();
    const Tile& rTile = rDefender.GetTile();
    // The intercepting side defends, so IsDefending and AttackerIsEmbarked conditions apply.
    // pUnit is the defender so SubjectIsOwner / SubjectIsForeign can gate attributed auras.
    EffectContext_t ctx{&rTile, CombatRole_t::Defender};
    ctx.pAttacker = &rAttacker;
    ctx.pUnit = &rDefender;
    std::vector<InterceptSource_t> sources;

    AppendMatchingIntercepts_(
        sources, rDefFaction.GetActiveEffects().effects, ctx,
        {EffectScope_t::FactionGlobal, EffectScope_t::AllOwnerBases},
        InterceptSourceKind_t::Building, [defenderId](const ActiveEffect_t& rEffect) {
            return InterceptBuildingIdentity_t{rEffect.sourceId, defenderId, std::nullopt};
        });

    const BaseManager* pBase = rState.FindBaseAt(rTile);
    if (pBase != nullptr && &pBase->GetFaction() == &rDefFaction)
    {
        const BaseId_t baseId = pBase->GetBaseId();
        AppendMatchingIntercepts_(
            sources, pBase->CollectBuildingEffects(), ctx, {EffectScope_t::ThisBase},
            InterceptSourceKind_t::Building, [defenderId, baseId](const ActiveEffect_t& rEffect) {
                return InterceptBuildingIdentity_t{rEffect.sourceId, defenderId, baseId};
            });
    }

    // ThisUnit, FactionUnits, and ProducedAtThisBase grants — each charges this defender.
    AppendMatchingIntercepts_(
        sources, CollectLiveUnitEffects(rDefender).effects, ctx,
        {EffectScope_t::ThisUnit, EffectScope_t::FactionUnits, EffectScope_t::ProducedAtThisBase},
        InterceptSourceKind_t::Unit, [defenderUnitId](const ActiveEffect_t& rEffect) {
            return InterceptUnitIdentity_t{rEffect.sourceId, defenderUnitId};
        });

    // The defender's composed pool holds peer copies of WorldGlobal charges, which its own
    // ledger could never ready; each grantor's local pool holds the real ones.
    for (const Faction& rGrantor : rState.Factions())
    {
        const FactionId_t grantorId = rGrantor.GetFactionId();
        AppendMatchingIntercepts_(
            sources, rGrantor.GetLocalActiveEffects().effects, ctx, {EffectScope_t::WorldGlobal},
            InterceptSourceKind_t::Building, [grantorId](const ActiveEffect_t& rEffect) {
                return InterceptBuildingIdentity_t{rEffect.sourceId, grantorId, std::nullopt};
            });
    }
    AppendMatchingIntercepts_(
        sources, rState.GetPlanetaryCouncil().CollectWorldEffects(), ctx,
        {EffectScope_t::WorldGlobal}, InterceptSourceKind_t::None,
        [](const ActiveEffect_t& rEffect) {
            return InterceptNoneIdentity_t{rEffect.sourceId};
        });

    AppendTileIntercepts_(sources, rTileEffects.CollectAreaEffects(rTile), ctx);
    return sources;
}

bool InterceptSourceReady(const InterceptSource_t& rSource, int missionYear)
{
    switch (rSource.deployKind)
    {
    case InterceptSourceKind_t::None:
        return true;
    case InterceptSourceKind_t::Unit:
    {
        const auto& rUnit = std::get<InterceptUnitIdentity_t>(rSource.identity);
        return InterceptUnit_(rUnit).Latches().IsInterceptReady(missionYear);
    }
    case InterceptSourceKind_t::Building:
    {
        const auto& rBuilding = std::get<InterceptBuildingIdentity_t>(rSource.identity);
        return Ledger_(rBuilding).CountReadyBuildings(rBuilding.sourceId, missionYear) > 0;
    }
    case InterceptSourceKind_t::Tile:
    {
        const auto& rTile = std::get<InterceptTileIdentity_t>(rSource.identity);
        return CurrentGameState().GetTileInterceptDeploys().IsReady(
            rTile.hostTile, rTile.sourceId, missionYear);
    }
    }
    throw std::logic_error("InterceptSourceReady: unknown source kind");
}

std::optional<BaseId_t> InterceptChargeBase(const InterceptSource_t& rSource)
{
    const auto* pBuilding = std::get_if<InterceptBuildingIdentity_t>(&rSource.identity);
    if (pBuilding == nullptr)
    {
        throw std::logic_error("InterceptChargeBase: source is not a building charge");
    }
    if (pBuilding->baseId)
    {
        return pBuilding->baseId;
    }
    const BaseManager* pBase = Ledger_(*pBuilding).FindBaseWithBuilding(pBuilding->sourceId);
    if (pBase == nullptr)
    {
        return std::nullopt;
    }
    return pBase->GetBaseId();
}

} // namespace ac
