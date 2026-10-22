#pragma once

#include "graphics/Graphics.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ac
{

// The layer an occupant draws in, in draw order. Names match JSON aside from case.
enum class ArtLayer_t
{
    Landform,
    Moisture,
    Rockiness,
    Landmark,
    Vegetation,
    River,
    Road,
    Object,
};

// Art paths per tile surface. A surface left out has no art there.
struct OccupantSpritePaths_t
{
    std::vector<std::string> land;
    std::vector<std::string> sea;

    const std::vector<std::string>& ForSurface(bool bWater) const
    {
        return bWater ? sea : land;
    }
};

// How a tile set picks its sprite from the tile's neighbors (see ui/TileAutotile.h). Edges and
// Blob match the JSON names aside from case; parse with magic_enum.
enum class SpriteTileLayout_t
{
    Edges,
    Blob,
    // SMAC's road network: mask 0 is the hub, 1–8 one link each toward the NW edge, N corner,
    // NE edge, E corner, SE edge, S corner, SW edge and W corner.
    Links,
};

// One sprite per neighbour mask (JSON "tiles", expanded at parse). A links set names its
// "link_occupants" and may draw over another network ("replaces_links_of").
struct OccupantTileSet_t
{
    SpriteTileLayout_t layout = SpriteTileLayout_t::Edges;
    OccupantSpritePaths_t paths;
    std::vector<std::string> linkOccupants;
    std::string replacesLinksOf;
};

// The yield an object sprite row follows.
enum class YieldStat_t
{
    Nutrients,
    Minerals,
    Energy,
};

// Object sprites by tile yield (JSON "yield_rows"): row = clamp(yield - 1, 0, rows - 1), as
// SMAC picks farm structures.
struct OccupantYieldRows_t
{
    YieldStat_t stat = YieldStat_t::Nutrients;
    OccupantSpritePaths_t paths;
};

// The palette steps a water art takes: its depth shade plus offset, kept within 0..max so the
// art stays on the water ramp.
struct WaterShadeRange_t
{
    int offset = 0;
    int max = 0;
};

// How an occupant draws on the world map: JSON "art" on a terrain or improvement entry.
struct OccupantArt_t
{
    ArtLayer_t layer = ArtLayer_t::Object;
    // JSON "variants" (one picked per tile from its coords), "tiles" or "yield_rows".
    std::variant<OccupantSpritePaths_t, OccupantTileSet_t, OccupantYieldRows_t> sprites;
    // Art drawn in place of the moisture layer, keyed by moisture name (Arid, Moist, Wet), one
    // variant per tile (SMAC's farm ground).
    std::unordered_map<std::string, std::vector<std::string>> ground;
    // Occupants whose object art is not drawn while this one is present (a soil enricher
    // replaces the farm structures).
    std::vector<std::string> hides;
    // How far an object reaches above the tile, as a fraction of the tile's height. Object
    // sprites (TER1.PCX: 100×62 over a 100×50 footprint) use 0.24.
    float overhang = 0.0f;
    // A landform on water: the shade range its art takes (see ui/WaterShading.h).
    std::optional<WaterShadeRange_t> depthShade;
    // Overrides the elevation fill and the minimap colour; the last occupant on a tile with one
    // wins.
    std::optional<Color_t> fillColor;
};

} // namespace ac
