#include "game/terraform/TerraformSpreadRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/WorldMap.h"
#include "game/terraform/TerraformSpreadCalculator.h"

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace ac
{

namespace
{

const TerraformSpreadCalculator& SpreadCalculator_()
{
    return *CurrentGameState().GetGameData().terraformSpreadCalculator;
}

// docs/thinker/terranx-map.md "Forest and kelp growth": arid and flatter tiles score higher.
int SpreadNeighborScore_(const Tile& rTile, const ImprovementSpread_t& rSpread)
{
    const int moisture = static_cast<int>(rTile.GetMoisture());
    const int rockiness = static_cast<int>(rTile.GetRockiness());
    return SpreadCalculator_().NeighborScore(rSpread.scoreFormula, moisture, rockiness);
}

bool MeetsAltitudeGate_(const Tile& rTile, const ImprovementSpread_t& rSpread)
{
    if (rTile.IsWater())
    {
        return rTile.GetElevation() >= rTile.MapRules().oceanShelfMeters;
    }
    if (!rSpread.altitudeLimitMeters)
    {
        throw std::logic_error(
            "MeetsAltitudeGate_: land spread requires altitude_limit_meters on the improvement");
    }
    if (rTile.GetElevation() <= *rSpread.altitudeLimitMeters)
    {
        return true;
    }
    return rTile.GetMoisture() != Moisture_t::Arid;
}

// TODO: whether SMAC's forest spread clears fungus off its target is unconfirmed. A fungus
// neighbour is not a legal target; clearing it would be an explicit removal, not an exception.
bool IsEligibleSpreadNeighbor_(const Tile& rNeighbor, bool bWantSea,
                               const ImprovementConfig_t& rConfig)
{
    const ImprovementSpread_t& rSpread = *rConfig.spreads;
    if (rNeighbor.IsWater() != bWantSea || !MeetsAltitudeGate_(rNeighbor, rSpread))
    {
        return false;
    }
    if (rNeighbor.HasImprovement(ImprovementIds::k_Base) || rNeighbor.HasImprovement(rConfig.id))
    {
        return false;
    }
    return CanBuildImprovement(rNeighbor, rConfig, {}, {});
}

const ImprovementConfig_t* SpreadingImprovementOn_(const Tile& rOrigin)
{
    for (const ImprovementConfig_t* pConfig : rOrigin.GetImprovements())
    {
        if (pConfig->spreads)
        {
            return pConfig;
        }
    }
    return nullptr;
}

std::optional<TerraformSpread_t> BestNeighborFor_(const Tile& rOrigin,
                                                  const ImprovementConfig_t& rConfig)
{
    const bool bWantSea = rOrigin.IsWater();
    const WorldMap& rWorldMap = CurrentGameState().GetWorldMap();
    const Tile* pBest = nullptr;
    int bestScore = 0;
    ForEachNeighborInChebyshevRadius(rOrigin, rWorldMap, 1,
        [&](const Tile* pNeighbor, int /*distance*/)
        {
            if (!IsEligibleSpreadNeighbor_(*pNeighbor, bWantSea, rConfig))
            {
                return;
            }
            const int score = SpreadNeighborScore_(*pNeighbor, *rConfig.spreads);
            if (pBest == nullptr || score > bestScore)
            {
                bestScore = score;
                pBest = pNeighbor;
            }
        });
    if (pBest == nullptr)
    {
        return std::nullopt;
    }
    return TerraformSpread_t{pBest, &rConfig};
}

} // namespace

int TerraformSpreadGrowthAttempts(int mapTileCount, int turnIndex)
{
    return SpreadCalculator_().GrowthAttempts(mapTileCount, turnIndex);
}

std::optional<TerraformSpread_t> PickSpreadTarget(const Tile& rOrigin)
{
    const ImprovementConfig_t* pConfig = SpreadingImprovementOn_(rOrigin);
    if (pConfig == nullptr)
    {
        return std::nullopt;
    }
    return BestNeighborFor_(rOrigin, *pConfig);
}

} // namespace ac
