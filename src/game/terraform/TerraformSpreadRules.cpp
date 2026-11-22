#include "game/terraform/TerraformSpreadRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <optional>

namespace ac
{

namespace
{

// Thinker/SMAC neighbor preference: arid and flatter tiles score higher.
int SpreadNeighborScore_(const Tile& rTile)
{
    const int moisture = static_cast<int>(rTile.GetMoisture());
    const int rockiness = static_cast<int>(rTile.GetRockiness());
    return 4 * (3 - moisture) - rockiness;
}

bool MeetsAltitudeGate_(const Tile& rTile)
{
    if (rTile.IsWater())
    {
        return rTile.GetElevation() >= rTile.MapRules().oceanShelfMeters;
    }
    if (rTile.GetElevation() <= rTile.MapRules().spreadAltitudeLimitMeters)
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
    if (rNeighbor.IsWater() != bWantSea || !MeetsAltitudeGate_(rNeighbor))
    {
        return false;
    }
    if (rNeighbor.HasImprovement(ImprovementIds::k_Base) || rNeighbor.HasImprovement(rConfig.id))
    {
        return false;
    }
    // SMAC never spreads forest onto rocky tiles.
    if (!bWantSea && rNeighbor.GetRockiness() == Rockiness_t::Rocky)
    {
        return false;
    }
    return CanBuildImprovement(rNeighbor, rConfig, {}, {});
}

} // namespace

int TerraformSpreadGrowthAttempts(int mapTileCount, int turnIndex)
{
    if (mapTileCount <= 0)
    {
        return 0;
    }
    const int turn = std::max(0, turnIndex);
    return mapTileCount / (turn / 4 + 32);
}

std::optional<TerraformSpread_t> PickSpreadTarget(const Tile& rOrigin)
{
    const bool bWantSea = rOrigin.IsWater();
    const std::string improvementId(bWantSea ? ImprovementIds::k_KelpFarm
                                             : ImprovementIds::k_Forest);
    if (!rOrigin.HasImprovement(improvementId))
    {
        return std::nullopt;
    }
    const GameState& rGame = CurrentGameState();
    const ImprovementRegistry& rImprovements = *rGame.GetGameData().improvementRegistry;
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    // Present by construction: ValidateTerrainFeatures proves both ids exist at load.
    const ImprovementConfig_t& rConfig = rImprovements.Get(improvementId);

    const Tile* pBest = nullptr;
    int bestScore = 0;
    ForEachNeighborInChebyshevRadius(rOrigin, rWorldMap, 1,
        [&](const Tile* pNeighbor, int /*distance*/)
        {
            if (!IsEligibleSpreadNeighbor_(*pNeighbor, bWantSea, rConfig))
            {
                return;
            }
            const int score = SpreadNeighborScore_(*pNeighbor);
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

} // namespace ac
