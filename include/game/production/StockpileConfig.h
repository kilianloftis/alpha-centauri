#pragma once

#include "game/production/IConstructable.h"
#include "game/effects/EffectConfig.h"
#include <algorithm>
#include <string>
#include <vector>

// Data only, separate from StockpileConfigParser.h so consumers do not pull <nlohmann/json.hpp>.

namespace ac
{

using StockpileId_t = std::string;

// How a fractional yield rounds. Required: it is a balance rule the parser will not invent.
enum class StockpileRounding_t
{
    Down,
    Up,
    Nearest,
};

// A stockpile item: each turn converts leftover minerals through its MineralsConverted
// modifiers. Not a BuildingConfig_t, since it is never constructed.
struct StockpileConfig_t : public IConstructable
{
    StockpileId_t id;
    std::string name;
    // Empty = always available (same convention as BuildingConfig_t).
    std::string requiredTech;
    // Empty-queue fallback rank, ties by load order, so a new config file cannot change it.
    int fallbackPriority = 0;
    StockpileRounding_t rounding = StockpileRounding_t::Down;
    std::vector<EffectConfig_t> effects;

    const std::string& GetId() const override { return id; }
    const std::string& GetName() const override { return name; }
    int GetBaseCost() const override { return 0; }
    ConstructableKind_t GetConstructableKind() const override
    {
        return ConstructableKind_t::Stockpile;
    }

    bool IsAvailable(const std::vector<std::string>& discoveredTechs) const
    {
        if (requiredTech.empty())
        {
            return true;
        }
        return std::find(discoveredTechs.begin(), discoveredTechs.end(), requiredTech)
               != discoveredTechs.end();
    }
};

} // namespace ac
