#pragma once

#include <vector>
#include <string>
#include <utility>

namespace ac
{

using FactionId = int;

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

} // namespace ac
