#include "game/combat/InterceptRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>

namespace ac
{

namespace
{

// The lane fixes the physical source; an effect that opts out of cooldowns (cooldownTurns < 0)
// has no ledger, so it may attempt on every attack.
void AppendMatchingIntercepts_(std::vector<InterceptSource_t>& rOut,
                               const std::vector<ActiveEffect_t>& rEffects,
                               const EffectContext_t& rCtx,
                               std::initializer_list<EffectScope_t> allowedScopes,
                               InterceptSourceKind_t sourceKind,
                               std::optional<BaseId_t> baseId,
                               FactionId_t ledgerFactionId)
{
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (std::find(allowedScopes.begin(), allowedScopes.end(), rEffect.config->scope)
                == allowedScopes.end())
        {
            continue;
        }
        const auto* pIntercept = std::get_if<InterceptEffect_t>(&rEffect.config->effect);
        if (!pIntercept || !ConditionSatisfied(*rEffect.config, rCtx, rEffect.originBase))
        {
            continue;
        }
        InterceptSource_t source;
        source.pIntercept = pIntercept;
        source.deployKind =
            pIntercept->cooldownTurns < 0 ? InterceptSourceKind_t::None : sourceKind;
        source.sourceKind = sourceKind;
        source.sourceId = rEffect.sourceId;
        source.baseId = baseId;
        source.ledgerFactionId = ledgerFactionId;
        rOut.push_back(source);
    }
}

const Faction& Ledger_(const InterceptSource_t& rSource)
{
    const Faction* pLedger = CurrentGameState().FindFaction(rSource.ledgerFactionId);
    if (pLedger == nullptr)
    {
        throw std::logic_error("Intercept source bills a faction that is not in the game");
    }
    return *pLedger;
}

} // namespace

std::vector<InterceptSource_t> CollectInterceptSources(const Unit& rAttacker,
                                                       const Unit& rDefender)
{
    const GameState& rState = CurrentGameState();
    const TileEffectsContext& rTileEffects = rState.GetTileEffects();
    const Faction& rDefFaction = rDefender.GetFaction();
    const FactionId_t defenderId = rDefFaction.GetFactionId();
    const Tile& rTile = rDefender.GetTile();
    // The intercepting side defends, so IsDefending and AttackerIsEmbarked conditions apply.
    EffectContext_t ctx{&rTile, CombatRole_t::Defender};
    ctx.pAttacker = &rAttacker;
    std::vector<InterceptSource_t> sources;

    AppendMatchingIntercepts_(sources, rDefFaction.GetActiveEffects().effects, ctx,
                              {EffectScope_t::FactionGlobal, EffectScope_t::AllOwnerBases},
                              InterceptSourceKind_t::Building, std::nullopt, defenderId);

    const BaseManager* pBase = rState.FindBaseAt(rTile.GetX(), rTile.GetY());
    if (pBase != nullptr && &pBase->GetFaction() == &rDefFaction)
    {
        AppendMatchingIntercepts_(sources, pBase->CollectBuildingEffects(), ctx,
                                  {EffectScope_t::ThisBase}, InterceptSourceKind_t::Building,
                                  pBase->GetBaseId(), defenderId);
    }

    AppendMatchingIntercepts_(sources, rDefender.GetDesign().CollectEffects(), ctx,
                              {EffectScope_t::ThisUnit}, InterceptSourceKind_t::Unit,
                              std::nullopt, defenderId);

    // The defender's composed pool holds peer copies of WorldGlobal charges, which its own
    // ledger could never ready; each grantor's local pool holds the real ones.
    for (const Faction& rGrantor : rState.Factions())
    {
        AppendMatchingIntercepts_(sources, rGrantor.GetLocalActiveEffects().effects, ctx,
                                  {EffectScope_t::WorldGlobal}, InterceptSourceKind_t::Building,
                                  std::nullopt, rGrantor.GetFactionId());
    }
    if (const PlanetaryCouncil* pCouncil = rState.GetPlanetaryCouncil())
    {
        AppendMatchingIntercepts_(sources, pCouncil->CollectWorldEffects(), ctx,
                                  {EffectScope_t::WorldGlobal}, InterceptSourceKind_t::None,
                                  std::nullopt, defenderId);
    }

    // TODO: a ThisTile source has no deploy ledger, so it ignores cooldownTurns; honouring it
    // needs a per-tile deploy record, and that rule is not defined yet.
    // TODO: tile sources ignore ActiveEffect_t::ownerFaction, so a hostile aura intercepts for
    // the defender. Whether a neutral or allied tile source may intercept is not settled.
    AppendMatchingIntercepts_(sources, rTileEffects.CollectAreaEffects(rTile), ctx,
                              {EffectScope_t::ThisTile}, InterceptSourceKind_t::None,
                              std::nullopt, defenderId);
    return sources;
}

bool InterceptSourceReady(const InterceptSource_t& rSource, const Unit& rDefender,
                          int missionYear)
{
    switch (rSource.deployKind)
    {
    case InterceptSourceKind_t::None:
        return true;
    case InterceptSourceKind_t::Unit:
        return rDefender.IsInterceptReady(missionYear);
    case InterceptSourceKind_t::Building:
        return Ledger_(rSource).CountReadyBuildings(rSource.sourceId, missionYear) > 0;
    }
    throw std::logic_error("InterceptSourceReady: unknown source kind");
}

std::optional<BaseId_t> InterceptChargeBase(const InterceptSource_t& rSource)
{
    if (rSource.baseId)
    {
        return rSource.baseId;
    }
    const BaseManager* pBase = Ledger_(rSource).FindBaseWithBuilding(rSource.sourceId);
    if (pBase == nullptr)
    {
        return std::nullopt;
    }
    return pBase->GetBaseId();
}

} // namespace ac
