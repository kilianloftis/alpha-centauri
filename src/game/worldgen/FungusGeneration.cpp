#include "game/worldgen/FungusGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <random>

namespace ac
{

namespace
{

constexpr int k_FungusFineShift = 4;

bool IsEligible_(const Tile& rTile, const ImprovementConfig_t& rFungus)
{
    if (rTile.HasTerrainFeature(rFungus.id))
    {
        return false;
    }
    return CanBuildImprovement(rTile, rFungus, {}, {});
}

bool InAnyBand_(int value, const FungusDecorationConfig_t& rConfig, int widening)
{
    for (const int start : rConfig.bandStarts)
    {
        const int lo = start - widening;
        const int hi = start + rConfig.bandWidth + 2 * widening;
        if (value >= lo && value <= hi)
        {
            return true;
        }
    }
    return false;
}

bool PoleRowRoll_(const Tile& rTile, int mapHeight, float chance, std::mt19937& rRng)
{
    if (rTile.GetY() != 0 && rTile.GetY() != mapHeight - 1)
    {
        return false;
    }
    if (chance <= 0.0f)
    {
        return false;
    }
    if (chance >= 1.0f)
    {
        return true;
    }
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    return unit(rRng) < chance;
}

} // namespace

void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig, int bandWidening,
                 const ImprovementConfig_t& rFungus, const SmacFractal& rFractal,
                 std::mt19937& rRng)
{
    if (rConfig.bandStarts.empty())
    {
        return;
    }

    const int height = rWorld.GetHeight();
    for (const std::unique_ptr<Tile>& pTile : rWorld.GetTiles())
    {
        Tile& rTile = *pTile;
        if (!IsEligible_(rTile, rFungus))
        {
            continue;
        }

        const int value = rFractal.Value(rTile.GetX(), rTile.GetY(), k_FungusFineShift);
        if (InAnyBand_(value, rConfig, bandWidening)
            || PoleRowRoll_(rTile, height, rConfig.poleRowChance, rRng))
        {
            rTile.AddTerrainFeature(rFungus);
        }
    }
}

} // namespace ac
