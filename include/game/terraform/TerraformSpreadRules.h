#pragma once

#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"

#include <optional>

namespace ac
{

struct TerraformSpread_t
{
    const Tile* pTarget = nullptr;
    const ImprovementConfig_t* pImprovement = nullptr;
};

// SMAC alien_fauna growth: MapAreaTiles / (turn/4 + 32) samples per turn, turnIndex counted
// from the first playable year.
int TerraformSpreadGrowthAttempts(int mapTileCount, int turnIndex);

// Forest spreads from land, KelpFarm from sea, onto the best eligible neighbour (arid and flat
// first). Empty when rOrigin holds neither or no neighbour qualifies.
std::optional<TerraformSpread_t> PickSpreadTarget(const Tile& rOrigin);

} // namespace ac
