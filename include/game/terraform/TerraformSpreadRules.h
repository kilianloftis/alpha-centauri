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

// Samples per turn from map_rules spread_attempts_formula. turnIndex is counted from the first
// playable year (docs/thinker/terranx-map.md "Forest and kelp growth").
int TerraformSpreadGrowthAttempts(int mapTileCount, int turnIndex);

// An improvement with a spreads object on rOrigin grows onto the best eligible neighbour.
// Empty when none spread or no neighbour qualifies.
std::optional<TerraformSpread_t> PickSpreadTarget(const Tile& rOrigin);

} // namespace ac
