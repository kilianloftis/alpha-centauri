#include "game/map/TerraformSpread.h"

#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <string>

namespace ac
{

namespace
{

// SMAC ocean-shelf / "one above sea" mapped onto our meter elevations: deep ocean
// below -2000m cannot host kelp; land above 1000m only receives forest when moist/wet.
constexpr int k_OceanShelfMinElevation = -2000;
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
    if (rNeighbor.HasImprovement("Base") || rNeighbor.HasImprovement(rConfig.id))
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

    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, 1, false,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            if (!pNeighbor || !IsEligibleSpreadNeighbor_(*pNeighbor, wantSea, rConfig))
            {
                return;
            }

            // Forest may land on fungus: clear temporarily so CanBuildImprovement's
            // Fungus exclude does not block, matching SMAC (fungus is wiped on spread).
            const bool hadFungus = !wantSea && pNeighbor->GetHasFungus();
            if (hadFungus)
            {
                pNeighbor->SetHasFungus(false);
            }
            const bool canBuild = CanBuildImprovement(*pNeighbor, rConfig);
            if (hadFungus)
            {
                pNeighbor->SetHasFungus(true);
            }
            if (!canBuild)
            {
                return;
            }

            const int score = SpreadNeighborScore_(*pNeighbor);
            if (score > bestScore)
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
    const char* improvementId = wantSea ? "KelpFarm" : "Forest";
    if (!rOrigin.HasImprovement(improvementId))
    {
        return false;
    }

    const ImprovementConfig_t* pConfig = rTileEffects.GetImprovements().Find(improvementId);
    if (!pConfig)
    {
        return false;
    }

    Tile* pTarget = PickBestSpreadNeighbor_(rOrigin, rWorldMap, *pConfig, wantSea);
    if (!pTarget)
    {
        return false;
    }

    if (!wantSea && pTarget->GetHasFungus())
    {
        pTarget->SetHasFungus(false);
    }
    rTileEffects.AddImprovementWithEffects(*pTarget, improvementId);
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
