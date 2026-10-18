#include "game/base/RebelRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/PopCompositionConfigParser.h"

#include <algorithm>
#include <cmath>

namespace ac
{

namespace
{

// Linear fade to zero at fadeRadius: adjacent is worth the most, fadeRadius away nothing.
int FadedBonus_(int distance, const RebelSelectionConfig_t& rConfig)
{
    return std::max(0, rConfig.fadeRadius - distance) * rConfig.distanceWeightPerTile;
}

int HqDistance_(const Faction& rFaction, const Tile& rFrom, int mapWidth,
                const RebelSelectionConfig_t& rConfig)
{
    const BaseManager* pHq = rFaction.GetHeadquarters();
    if (!pHq)
    {
        return rConfig.missingHqDistance;
    }
    return ChebyshevDistance(rFrom, pHq->GetTile(), mapWidth);
}

int DistanceBonus_(const Faction& rCandidate, const BaseManager& rRebelling,
                   const RebelSelectionConfig_t& rConfig, int mapWidth)
{
    const Tile& rFrom = rRebelling.GetTile();
    switch (rConfig.distanceMode)
    {
        case RebelDistanceMode_t::None:
            return 0;
        case RebelDistanceMode_t::NearestBase:
        {
            // Candidates are pre-filtered to factions with at least one base, so this is set.
            const BaseManager* pNearest = rCandidate.FindNearestBase(rFrom);
            return FadedBonus_(ChebyshevDistance(rFrom, pNearest->GetTile(), mapWidth), rConfig);
        }
        case RebelDistanceMode_t::HqDistance:
            return FadedBonus_(HqDistance_(rCandidate, rFrom, mapWidth, rConfig), rConfig);
        case RebelDistanceMode_t::NearbyBases:
        {
            int total = 0;
            for (const BaseManager& rBase : rCandidate.Bases())
            {
                total += FadedBonus_(ChebyshevDistance(rFrom, rBase.GetTile(), mapWidth),
                                     rConfig);
            }
            return total;
        }
    }
    return 0;
}

int RebelJoinWeight_(const Faction& rFaction, const RebelSelectionConfig_t& rConfig)
{
    const double seed = static_cast<double>(rConfig.baseJoinWeight);
    const double weight =
        ResolveFactionStat(rFaction.GetActiveEffects(), StatId_t::RebelJoinWeight, seed);
    return static_cast<int>(std::lround(weight));
}

} // namespace

std::vector<RebelCandidate_t> RebelCandidateWeights(const BaseManager& rRebelling)
{
    const GameState& rGameState = CurrentGameState();
    const RebelSelectionConfig_t& rConfig =
        rGameState.GetGameData().popCompositionConfig->rebelSelection;
    const FactionId_t ownerId = rRebelling.GetFaction().GetFactionId();
    const int mapWidth = rGameState.GetWorldMap().GetWidth();

    std::vector<RebelCandidate_t> candidates;
    for (const Faction& rFaction : rGameState.Factions())
    {
        if (rFaction.GetFactionId() == ownerId)
        {
            continue;
        }
        // Alive = still holds at least one base (factions are never removed from GameState).
        if (rFaction.GetBaseCount() == 0)
        {
            continue;
        }

        const int weight = RebelJoinWeight_(rFaction, rConfig)
            + DistanceBonus_(rFaction, rRebelling, rConfig, mapWidth);
        if (weight <= 0)
        {
            continue;
        }
        candidates.push_back(RebelCandidate_t{rFaction.GetFactionId(), weight});
    }
    return candidates;
}

} // namespace ac
