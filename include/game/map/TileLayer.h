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

inline constexpr size_t k_TileLayerCount = static_cast<size_t>(TileLayerType_t::Count);

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
    // Landform layer (Layer 0) — sea depth bands; flat land is empty (moisture carries art).
    inline const std::string k_Water = "water";
    inline const std::string k_Flat = "flat";

    // Moisture layer (Layer 1)
    inline const std::string k_Arid = "arid";
    inline const std::string k_Moist = "moist";
    inline const std::string k_Wet = "wet";

    // Rockiness layer (Layer 2) — keyed overlays drawn above moisture bases.
    inline const std::string k_Rolling = "rolling";
    inline const std::string k_Rocky = "rocky";

    // Vegetation layer (Layer 3) — fungus replaces farm/forest visually when present.
    inline const std::string k_Farm = "farm";
    inline const std::string k_Forest = "forest";
    inline const std::string k_Fungus = "fungus";

    // Road layer (Layer 4)
    inline const std::string k_Road = "road";
} // namespace TileLayerContent

} // namespace ac
