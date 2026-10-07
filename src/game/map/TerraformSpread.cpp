#include "game/map/TerraformSpread.h"

#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/OccupantCoexistence.h"
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
        return rTile.GetElevation() >= rTile.MapRules().oceanShelfMeters;
    }
    if (rTile.GetElevation() <= rTile.MapRules().spreadAltitudeLimitMeters)
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

// TODO: forest spread used to wipe fungus off the tile it spread onto, citing SMAC. Fungus
// is terrain now and improvements no longer displace terrain, so a fungus neighbour is simply
// not a legal target. Nobody has confirmed which of the two SMAC actually does — if spread is
// meant to clear fungus, that belongs here as an explicit removal, not as a silent exception
// to "improvements never remove terrain".
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
            if (!CanBuildImprovement(*pNeighbor, rConfig))
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

    rTileEffects.AddOccupantWithEffects(*pTarget, std::string(improvementId));
    return true;
}

void SpreadTerraformImprovements(WorldMap& rWorldMap, TileEffectsContext& rTileEffects,
                                 int turnIndex, std::mt19937& rRng)
{
    const auto tiles = rWorldMap.GetTiles();
    if (tiles.empty())
    {
        return;
    }

    const int tileCount = static_cast<int>(tiles.size());
    const int attempts = TerraformSpreadGrowthAttempts(tileCount, turnIndex);
    std::uniform_int_distribution<int> dist(0, tileCount - 1);

    for (int iter = 0; iter < attempts; ++iter)
    {
        Tile* pTile = tiles[static_cast<size_t>(dist(rRng))].get();
        if (!pTile)
        {
            continue;
        }
        TrySpreadTerraformFromTile(*pTile, rWorldMap, rTileEffects);
    }
}

} // namespace ac
