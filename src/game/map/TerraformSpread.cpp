#include "game/map/TerraformSpread.h"

#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace ac
{

namespace
{

// Land above 1000m only receives forest when moist/wet. Deep-ocean kelp gate uses
// k_OceanShelfMinElevation from Tile.h.
constexpr int k_OneAboveSeaMaxElevation = 1000;

int MoistureOrdinal_(Moisture_t moisture)
{
    return static_cast<int>(moisture);
}

int RockinessOrdinal_(Rockiness_t rockiness)
{
    return static_cast<int>(rockiness);
}

// Thinker/SMAC neighbor preference: arid and flatter tiles score higher.
int SpreadNeighborScore_(const Tile& rTile)
{
    return 4 * (3 - MoistureOrdinal_(rTile.GetMoisture())) - RockinessOrdinal_(rTile.GetRockiness());
}

bool MeetsAltitudeGate_(const Tile& rTile)
{
    if (rTile.IsWater())
    {
        return rTile.GetElevation() >= k_OceanShelfMinElevation;
    }
    if (rTile.GetElevation() <= k_OneAboveSeaMaxElevation)
    {
        return true;
    }
    return rTile.GetMoisture() != Moisture_t::Arid;
}

bool IsEligibleSpreadNeighbor_(const Tile& rNeighbor, bool wantSea,
                               const ImprovementConfig_t& rConfig)
{
    if (rNeighbor.IsWater() != wantSea)
    {
        return false;
    }
    if (!MeetsAltitudeGate_(rNeighbor))
    {
        return false;
    }
    if (rNeighbor.HasImprovement(ImprovementIds::k_Base) || rNeighbor.HasImprovement(rConfig.id))
    {
        return false;
    }
    // SMAC never spreads forest onto rocky tiles.
    if (!wantSea && rNeighbor.GetRockiness() == Rockiness_t::Rocky)
    {
        return false;
    }
    return true;
}

Tile* PickBestSpreadNeighbor_(Tile& rOrigin, WorldMap& rWorldMap,
                              const ImprovementConfig_t& rConfig, bool wantSea)
{
    Tile* pBest = nullptr;
    int bestScore = 0;

    // Forest spread wipes fungus (SMAC), so Fungus must not block eligibility.
    const std::string_view clearedFeature = wantSea ? std::string_view{} : ImprovementIds::k_Fungus;

    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, 1, false,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            if (!pNeighbor || !IsEligibleSpreadNeighbor_(*pNeighbor, wantSea, rConfig))
            {
                return;
            }
            if (!CanBuildImprovement(*pNeighbor, rConfig, clearedFeature))
            {
                return;
            }

            const int score = SpreadNeighborScore_(*pNeighbor);
            if (!pBest || score > bestScore)
            {
                bestScore = score;
                pBest = pNeighbor;
            }
        });

    return pBest;
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

bool TrySpreadTerraformFromTile(Tile& rOrigin, WorldMap& rWorldMap,
                                TileEffectsContext& rTileEffects)
{
    const bool wantSea = rOrigin.IsWater();
    const std::string_view improvementId =
        wantSea ? ImprovementIds::k_KelpFarm : ImprovementIds::k_Forest;
    if (!rOrigin.HasImprovement(improvementId))
    {
        return false;
    }

    // Present by construction: ValidateTerrainFeatures proves both ids exist at load.
    const ImprovementConfig_t& rConfig =
        rTileEffects.GetImprovements().Get(std::string(improvementId));

    Tile* pTarget = PickBestSpreadNeighbor_(rOrigin, rWorldMap, rConfig, wantSea);
    if (!pTarget)
    {
        return false;
    }

    if (!wantSea && pTarget->GetHasFungus())
    {
        pTarget->SetHasFungus(false);
    }
    rTileEffects.AddImprovementWithEffects(*pTarget, std::string(improvementId));
    return true;
}

void SpreadTerraformImprovements(WorldMap& rWorldMap, TileEffectsContext& rTileEffects,
                                 int turnIndex, std::mt19937& rRng)
{
    const int width = rWorldMap.GetWidth();
    const int height = rWorldMap.GetHeight();
    if (width <= 0 || height <= 0)
    {
        return;
    }

    const int attempts = TerraformSpreadGrowthAttempts(width * height, turnIndex);
    std::uniform_int_distribution<int> distX(0, width - 1);
    std::uniform_int_distribution<int> distY(0, height - 1);

    for (int iter = 0; iter < attempts; ++iter)
    {
        Tile* pTile = rWorldMap.GetTile(distX(rRng), distY(rRng));
        if (!pTile)
        {
            continue;
        }
        TrySpreadTerraformFromTile(*pTile, rWorldMap, rTileEffects);
    }
}

} // namespace ac
