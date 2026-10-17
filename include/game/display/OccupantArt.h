#pragma once

#include "graphics/Graphics.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ac
{

// The layer an occupant's art draws in. Enumerators run in draw order; Object art draws after
// the terrain layers (see TileRenderer). Matches the JSON names aside from case; parse with
// magic_enum.
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

// Art drawn one sprite per neighbor mask: JSON "tiles": {"layout": "edges" | "blob" | "links",
// "land": "...{mask}...", "sea": "..."}. The pattern is expanded at parse, so paths[mask] is the
// sprite for that mask and an empty list means no art on that surface. A links set also names
// "link_occupants" (a tile carrying any of them joins the network; Base carries every network)
// and may name "replaces_links_of": the network whose link it draws over where both tiles carry
// this one (a mag tube over a road).
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

// Object sprites picked by the tile's yield: JSON "yield_rows": {"stat": "nutrients",
// "land": [...], "sea": [...]}; row = clamp(yield - 1, 0, rows - 1), as SMAC picks its farm
// structures.
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
