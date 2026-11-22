#pragma once

#include "game/base/BaseTypes.h"
#include "game/map/MapRulesConfig.h"
#include "game/map/Tile.h"

#include <span>
#include <vector>

namespace ac
{

class Unit;

struct ElevationSeed_t
{
    const Tile* pTile = nullptr;
    int deltaMeters = 0;
};

struct ElevationWrite_t
{
    const Tile* pTile = nullptr;
    int elevation = 0;
};

// These read the elevation rules of the world the tile or unit stands in.
int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId);

// Elevation after a max-sized level change in that direction, clamped to Planet's range.
int FormerElevationAfterMaxLevel(const Tile& rTile, bool bRaise, const MapRulesConfig_t& rRules);

// True when that change moves the tile, is not raise from the top altitude band (TOOHIGH),
// and the enter grid still allows this Former on the resulting surface.
// docs/thinker/terranx-map.md "Starting a project".
bool FormerElevationChangeAllowed(const Unit& rUnit, bool bRaise, const MapRulesConfig_t& rRules);

// Every write a change makes, in order: seeds move within the band and Planet's range, then each
// moved tile pulls its neighbours within the slope limit. Throws on a band outside Planet's range.
std::vector<ElevationWrite_t> PlanElevationChange(std::span<const ElevationSeed_t> seeds,
                                                  int floorMeters, int ceilingMeters,
                                                  const MapRulesConfig_t& rRules);

// The sum of the level rolls, capped at Planet's full elevation span.
int EarthquakeDeltaMeters(std::span<const int> levelRollsMeters,
                          const MapRulesConfig_t& rRules);

} // namespace ac
