#pragma once

#include <vector>
#include <string>
#include <utility>

namespace ac
{

using FactionId_t = int;
using BaseId_t = int;

struct TradeRoute_t
{
    int energyBonus;
    int targetFactionId;
    int targetBaseId;
};

struct TileResources_t
{
    int nutrients;
    int energy;
    int minerals;
};

// Per-tile yield as shown to the UI / collected in production.
// `effective` honours MaxClamp / MinClamp on the pre-bypass lane; `potential` is the same
// resolve with those clamps stripped (resource-bonus bypass_clamp contributions are in both).
struct TileYieldView_t
{
    TileResources_t effective;
    TileResources_t potential;
};

// (x, y) map coordinate used for tile assignments.
// {-1, -1} means unassigned.
using TileCoord_t = std::pair<int, int>;

// Riot / golden-age state as it crosses a save or a base transfer. One struct rather than
// loose snapshot fields so adding a mood flag cannot be captured without being restored.
struct MoodState_t
{
    bool bRioting = false;
    bool bPendingRiot = false;
    int forcedRiotTurnsRemaining = 0;
    int consecutiveRiotTurns = 0;
    bool bInGoldenAge = false;
    bool bPendingGoldenAge = false;
};

} // namespace ac
