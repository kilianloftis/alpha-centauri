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
// `effective` applies TileResourceCap; `potential` is the same resolve without caps
// (resource-bonus apply_after_restriction contributions are in both).
struct TileYieldView_t
{
    TileResources_t effective;
    TileResources_t potential;
};

// (x, y) map coordinate used for tile assignments.
// {-1, -1} means unassigned.
using TileCoord_t = std::pair<int, int>;

} // namespace ac
