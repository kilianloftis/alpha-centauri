#include "game/faction/UnitVisibility.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/BonusEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

namespace
{

bool AppliesForFaction_(const ActiveEffect_t& rEffect, FactionId_t forFaction)
{
    return !rEffect.ownerFaction.has_value() || *rEffect.ownerFaction == forFaction;
}

void CollectConcealmentChannels_(const Unit& rSubject, const TileEffectsContext& rTileEffects,
                                 std::unordered_set<std::string>& rOut)
{
    const EffectContext_t ctx{&rSubject.GetTile()};

    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rSubject))
    {
        if (!ConditionSatisfied(*rEffect.config, ctx, rEffect.originBase))
        {
            continue;
        }
        const ConcealEffect_t* pConceal = std::get_if<ConcealEffect_t>(&rEffect.config->effect);
        if (pConceal)
        {
            rOut.insert(pConceal->channel);
        }
    }

    for (const ActiveEffect_t& rEffect : rTileEffects.CollectAreaEffects(rSubject.GetTile()))
    {
        if (!ConditionSatisfied(*rEffect.config, ctx, rEffect.originBase))
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
                           const TileEffectsContext& rTileEffects)
{
    const FactionId_t observerId = rObserver.GetFactionId();
    const EffectContext_t ctx{&rTile};
    for (const ActiveEffect_t& rEffect : rTileEffects.CollectAreaEffects(rTile))
    {
        if (!AppliesForFaction_(rEffect, observerId)
            || !ConditionSatisfied(*rEffect.config, ctx, rEffect.originBase))
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

} // namespace

bool IsUnitVisibleTo(const Faction& rObserver, const Unit& rSubject,
                     const TileEffectsContext& rTileEffects)
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

    const Tile& rTile = rSubject.GetTile();
    if (rObserver.GetVisibleMap().IsSized() && !rObserver.GetVisibleMap().IsVisible(rTile))
    {
        return false;
    }

    std::unordered_set<std::string> channels;
    CollectConcealmentChannels_(rSubject, rTileEffects, channels);
    for (const std::string& rChannel : channels)
    {
        if (!HasDetectionCovering_(rObserver, rTile, rChannel, rTileEffects))
        {
            return false;
        }
    }
    return true;
}

} // namespace ac
