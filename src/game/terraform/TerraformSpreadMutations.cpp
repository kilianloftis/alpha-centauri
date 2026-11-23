#include "game/terraform/TerraformSpreadMutations.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/terraform/TerraformSpreadRules.h"
#include "lib/RandomRoll.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace ac
{

bool TrySpreadTerraformFromTile(Tile& rOrigin, WorldMap& rWorldMap,
                                TileEffectsContext& rTileEffects)
{
    RequireCurrentWorldMap(rWorldMap);
    const std::optional<TerraformSpread_t> spread = PickSpreadTarget(rOrigin);
    if (!spread)
    {
        return false;
    }
    Tile* pTarget = rWorldMap.GetTile(spread->pTarget->GetCoord());
    if (pTarget == nullptr)
    {
        throw std::logic_error("TrySpreadTerraformFromTile: spread target is off this world");
    }
    // docs/thinker/terranx-map.md "Forest and kelp growth": the fungus bit is cleared.
    if (pTarget->HasFeature(ImprovementIds::k_Fungus))
    {
        rTileEffects.RemoveOccupantWithEffects(*pTarget, std::string(ImprovementIds::k_Fungus));
    }
    rTileEffects.AddOccupantWithEffects(*pTarget, spread->pImprovement->id);
    return true;
}

void SpreadTerraformImprovements(WorldMap& rWorldMap, TileEffectsContext& rTileEffects,
                                 int turnIndex, std::mt19937& rRng)
{
    RequireCurrentWorldMap(rWorldMap);
    const auto tiles = rWorldMap.GetTiles();
    if (tiles.empty())
    {
        return;
    }

    const int tileCount = static_cast<int>(tiles.size());
    const int attempts = TerraformSpreadGrowthAttempts(tileCount, turnIndex);

    for (int iter = 0; iter < attempts; ++iter)
    {
        Tile* pTile = tiles[PickUniformIndex(tiles.size(), rRng)].get();
        if (!pTile)
        {
            continue;
        }
        TrySpreadTerraformFromTile(*pTile, rWorldMap, rTileEffects);
    }
}

} // namespace ac
