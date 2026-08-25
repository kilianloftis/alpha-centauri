#include "game/faction/UnitVisibility.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/FactionVisibleMap.h"
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
    const EffectContext_t ctx{&rSubject.GetTile()};

    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rSubject).effects)
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

    // Tile-area Conceal: terrain (unset owner) applies to everyone; unit/territory auras
    // only conceal subjects matching ownerFaction.
    const FactionId_t subjectId = rSubject.GetFaction().GetFactionId();
    for (const ActiveEffect_t& rEffect : areaEffects)
    {
        if (!AppliesForFaction(rEffect, subjectId)
            || !ConditionSatisfied(*rEffect.config, ctx, rEffect.originBase))
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
    const FactionId_t observerId = rObserver.GetFactionId();
    const EffectContext_t ctx{&rTile};
    for (const ActiveEffect_t& rEffect : areaEffects)
    {
        // Detect fails closed without a stamped owner — unset never pierces for every faction.
        if (!rEffect.ownerFaction.has_value()
            || !AppliesForFaction(rEffect, observerId)
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
    if (!rObserver.GetVisibleMap().IsVisible(rTile))
    {
        return false;
    }

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

} // namespace ac
