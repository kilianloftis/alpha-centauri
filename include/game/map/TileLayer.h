#pragma once

#include <optional>
#include <string>

namespace ac
{

// Visual layers for rendering a tile, ordered from bottom to top.
// Each layer holds at most one content identifier, which maps to a sprite
// or other visual representation. Empty layers are represented by std::nullopt.
enum class TileLayerType_t
{
    Landform = 0,
    Moisture = 1,
    Rockiness = 2,
    Vegetation = 3,
    Road = 4,
    Improvement = 5,
    Count
};

inline constexpr size_t k_tileLayerCount = static_cast<size_t>(TileLayerType_t::Count);

struct TileLayer_t
{
    TileLayer_t(TileLayerType_t type, std::optional<std::string> contentId)
        : type(type)
        , contentId(std::move(contentId))
    {
    }

    TileLayerType_t type;
    std::optional<std::string> contentId;
};

// Content identifiers for built-in layer contents.
// Mods may add additional content IDs for custom layers.
namespace TileLayerContent
{
    // Landform layer (Layer 0)
    inline const std::string k_water = "water";
    inline const std::string k_flat = "flat";
    inline const std::string k_rolling = "rolling";

    // Moisture layer (Layer 1)
    inline const std::string k_arid = "arid";
    inline const std::string k_moist = "moist";
    inline const std::string k_wet = "wet";

    // Rockiness layer (Layer 2)
    inline const std::string k_rocky = "rocky";

    // Vegetation layer (Layer 3)
    inline const std::string k_farm = "farm";
    inline const std::string k_forest = "forest";

    // Road layer (Layer 4)
    inline const std::string k_road = "road";
} // namespace TileLayerContent

} // namespace ac
