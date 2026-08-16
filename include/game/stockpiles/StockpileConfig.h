#pragma once

#include "game/IConstructable.h"
#include "game/effects/EffectConfig.h"
#include <algorithm>
#include <string>
#include <vector>

// Data only, separate from StockpileConfigParser.h so consumers do not pull <nlohmann/json.hpp>.

namespace ac
{

using StockpileId_t = std::string;

// How a fractional per-stat yield becomes an integer. Required in config: a conversion rate
// of 0.5 says nothing about what 5 minerals produce until the rounding is stated, and this
// is a balance rule, so the parser refuses to invent one. Stock Stockpile_Energy rounds up.
enum class StockpileRounding_t
{
    Down,
    Up,
    Nearest,
};

// A never-completing production item. Each turn the MineralConversion stage feeds the base's
// leftover minerals through the MineralsConverted StatModifiers in `effects` and credits the
// results. Selectable from the build menu and used as the empty-queue fallback.
//
// Deliberately not a BuildingConfig_t: a stockpile is never constructed, so cost, upkeep,
// allow_multiple, secret_project and orbital have no meaning for it. Keeping it a separate
// type is what lets those fields simply not exist, rather than existing and being rejected.
struct StockpileConfig_t : public IConstructable
{
    StockpileId_t id;
    std::string name;
    // Empty = always available (same convention as BuildingConfig_t).
    std::string requiredTech;
    // Empty-queue fallback ranking: among the stockpiles whose tech is discovered, the
    // highest priority wins, ties broken by load order. Explicit so that adding a config
    // file cannot silently change which item every base defaults to.
    int fallbackPriority = 0;
    StockpileRounding_t rounding = StockpileRounding_t::Down;
    std::vector<EffectConfig_t> effects;

    const std::string& GetId() const override { return id; }
    const std::string& GetName() const override { return name; }
    int GetBaseCost() const override { return 0; }
    bool NeverCompletes() const override { return true; }
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
