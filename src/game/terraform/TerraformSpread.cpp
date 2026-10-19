#include "game/terraform/TerraformSpread.h"

#include "game/effects/TileEffectsContext.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/terraform/TerraformRules.h"

#include <optional>
#include <stdexcept>

namespace ac
{

bool TrySpreadTerraformFromTile(Tile& rOrigin, WorldMap& rWorldMap,
                                TileEffectsContext& rTileEffects)
{
    const std::optional<TerraformSpread_t> spread = PickSpreadTarget(rOrigin);
    if (!spread)
    {
        return false;
    }
    Tile* pTarget = rWorldMap.GetTile(spread->pTarget->GetX(), spread->pTarget->GetY());
    if (pTarget == nullptr)
    {
        throw std::logic_error("TrySpreadTerraformFromTile: spread target is off this world");
    }
    rTileEffects.AddOccupantWithEffects(*pTarget, spread->pImprovement->id);
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
