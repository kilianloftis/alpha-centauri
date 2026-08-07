#pragma once

#include <string_view>

namespace ac
{

// ImprovementConfig_t::id values from config/improvements.json that C++ references by name.
// These are the gameplay/rules domain, matched by Tile::HasFeature and CanBuildImprovement.
// Sprite/atlas keys live in TileLayerContent (TileLayer.h) and are a different, lowercase domain
// - the two must not be swapped.
namespace ImprovementIds
{
    inline constexpr std::string_view k_Farm = "Farm";
    inline constexpr std::string_view k_Forest = "Forest";
    inline constexpr std::string_view k_KelpFarm = "KelpFarm";
    inline constexpr std::string_view k_Road = "Road";
    inline constexpr std::string_view k_Base = "Base";

    // A TerrainFeature_t, not a placeable improvement: HasFeature resolves it, HasImprovement
    // never will.
    inline constexpr std::string_view k_Fungus = "Fungus";
} // namespace ImprovementIds

} // namespace ac
