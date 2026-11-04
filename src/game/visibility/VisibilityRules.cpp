#include "game/visibility/VisibilityRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TileEffectsContext.h"
#include "game/visibility/FactionRevealedUnits.h"
#include "game/visibility/FactionVisibleMap.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"

#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

namespace
{

void CollectConcealmentChannels_(const Unit& rSubject, std::span<const ActiveEffect_t> areaEffects,
                                 std::unordered_set<std::string>& rOut)
{
    EffectContext_t ctx{&rSubject.GetTile()};
    ctx.pUnit = &rSubject;

    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rSubject).effects)
    {
        if (!ConditionSatisfied(*rEffect.config, ActiveEffectContext(rEffect, ctx),
                                rEffect.originBase))
        {
            continue;
        }
        const ConcealEffect_t* pConceal = std::get_if<ConcealEffect_t>(&rEffect.config->effect);
        if (pConceal)
        {
            rOut.insert(pConceal->channel);
        }
    }

    // Tile-area Conceal: universal entries have no polarity condition; attributed auras
    // author SubjectIsOwner.
    for (const ActiveEffect_t& rEffect : areaEffects)
    {
        if (!ConditionSatisfied(*rEffect.config, ActiveEffectContext(rEffect, ctx),
                                rEffect.originBase))
        {
            continue;
        }
        const ConcealEffect_t* pConceal = std::get_if<ConcealEffect_t>(&rEffect.config->effect);
        if (pConceal)
        {
            rOut.insert(pConceal->channel);
        }
    }
}

bool HasDetectionCovering_(const Faction& rObserver, const Tile& rTile, const std::string& rChannel,
                           std::span<const ActiveEffect_t> areaEffects)
{
    EffectContext_t ctx{&rTile};
    ctx.pFaction = &rObserver;
    for (const ActiveEffect_t& rEffect : areaEffects)
    {
        // Detect fails closed without a stamped owner — unset never pierces for every faction.
        if (!rEffect.ownerFaction.has_value()
            || !ConditionSatisfied(*rEffect.config, ActiveEffectContext(rEffect, ctx),
                                   rEffect.originBase))
        {
            continue;
        }
        const DetectEffect_t* pDetect = std::get_if<DetectEffect_t>(&rEffect.config->effect);
        if (pDetect && pDetect->channel == rChannel)
        {
            return true;
        }
    }
    return false;
}

// The tile is in view, or explored and the subject shows through fog.
bool InSight_(const Faction& rObserver, const Unit& rSubject)
{
    const Tile& rTile = rSubject.GetTile();
    if (rObserver.GetVisibleMap().IsVisible(rTile))
    {
        return true;
    }
    return ResolveFlag(rSubject, RuleFlagId_t::VisibleInFog)
        && rObserver.GetExploredMap().IsExplored(rTile);
}

bool DetectsEveryConcealment_(const Faction& rObserver, const Unit& rSubject)
{
    const Tile& rTile = rSubject.GetTile();
    const TileEffectsContext& rTileEffects = CurrentGameState().GetTileEffects();
    const std::vector<ActiveEffect_t> areaEffects = rTileEffects.CollectAreaEffects(rTile);
    std::unordered_set<std::string> channels;
    CollectConcealmentChannels_(rSubject, areaEffects, channels);
    for (const std::string& rChannel : channels)
    {
        if (!HasDetectionCovering_(rObserver, rTile, rChannel, areaEffects))
        {
            return false;
        }
    }
    return true;
}

} // namespace

bool IsUnitVisibleTo(const Faction& rObserver, const Unit& rSubject)
{
    if (rSubject.GetFaction().GetFactionId() == rObserver.GetFactionId())
    {
        return true;
    }

    // Contact reveal (blocked move / ZOC bump) pierces fog and Conceal for this observer.
    if (rObserver.GetRevealedUnits().IsRevealed(rSubject))
    {
        return true;
    }

    return InSight_(rObserver, rSubject) && DetectsEveryConcealment_(rObserver, rSubject);
}

bool IsOwnerKnownTo(FactionId_t observer, const Unit& rSubject)
{
    return rSubject.GetFaction().GetFactionId() == observer
        || !ResolveFlag(rSubject, RuleFlagId_t::Covert);
}

} // namespace ac
