#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantArt.h"
#include "graphics/Graphics.h"
#include "ui/world/MapAppearance.h"

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

class SpriteLibrary;
class Tile;
class WorldMap;
struct CoastCornerArt_t;
struct ImprovementConfig_t;
struct TileRendererStyle_t;

// Stable variant index for a tile cell. Depends only on coords, content id, and count
// (config list order is the contract). Returns 0 when count is 0.
size_t PickSpriteIndex(int tileX, int tileY, std::string_view contentId, size_t count);

// paths[PickSpriteIndex(...)]; empty paths → empty string (caller skips the draw).
const std::string& PickSpritePath(const std::vector<std::string>& paths, int tileX, int tileY,
                                  std::string_view contentId);

// Paints one tile for MapRenderer, as the MapAppearance shows it to its viewer.
// Fills by an occupant's fill_color or elevation (blue water / brown land), then draws each
// occupant's art by its ArtLayer_t, bottom to top. The occupants a tile draws, and those its
// neighbors count for tile sets and road networks, come from the MapAppearance: remembered on
// tiles out of sight. Elevation and surface (coast, water shading) are always the live tile's.
// The appearance's cover decides the rest: shroud draws only the shroud colour; fog draws land
// art fog_land_shade steps darker under a haze, with object sprites clear on top. Terrain art is
// palette indices shaded as SMAC does: land by its relief shades, water per vertex by depth. Art
// that fails to load draws nothing under terrain (the fill shows) and a checker for objects.
// Footprint is a 2:1 isometric diamond (width = size, height = size / 2).
// When an art's variants list multiple assets, PickSpritePath chooses one per tile. Sprites load
// through the SpriteLibrary the renderer was built over; the style is the one it was built with.
class TileRenderer
{
public:
    // The tile's yield, for object sprites picked by yield (farm structures). Without one they
    // take their first row.
    using YieldLookup_t = std::function<TileResources_t(const Tile&)>;

    TileRenderer(SpriteLibrary& rSprites, const TileRendererStyle_t& rStyle);

    // The tile's one colour: the shroud colour, else the last occupant fill_color on the tile or
    // the elevation gradient, dimmed under fog.
    Color_t FillColor(const Tile& rTile, const MapAppearance& rAppearance) const;

    // A tile's two halves, so MapRenderer can draw its grid lines between them as SMAC does.
    // RenderTerrain draws the fill, the landform to vegetation layers (an improvement's ground in
    // place of the moisture base), coast, river, road networks and fog haze on rShape's four
    // triangles (land at its vertex shades); under shroud, only the shroud. RenderObjects draws
    // the object art of terrain occupants and then of improvements (skipping those another
    // occupant hides) on a flat footprint seated at the mean of the shape's four corners, and
    // nothing under shroud. An object whose configured art is missing draws a magenta and black
    // checker in its place.
    void RenderTerrain(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                       const MapAppearance& rAppearance) const;
    void RenderObjects(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                       const MapAppearance& rAppearance, const YieldLookup_t& rYieldOf = {}) const;

private:
    static constexpr std::size_t k_CoastPartCount = 2;
    static constexpr std::size_t k_CoastCaseCount = 8;
    using CoastPaths_t = std::array<std::array<std::array<std::string, k_CoastCaseCount>,
                                               k_DiamondCornerCount>,
                                    k_CoastPartCount>;

    static CoastPaths_t BuildCoastPaths_(const std::string& rDirectory);

    bool TryDrawSprite_(Graphics& rGraphics, const std::string& path, float x, float y,
                        float width, float height, const Color_t& tint) const;
    bool TryDrawTileSprite_(Graphics& rGraphics, const std::string& path,
                            const TileShape_t& rShape) const;
    TileShape_t TerrainShape_(const TileShape_t& rShape, const Tile& rTile,
                              TileCover_t cover) const;
    const std::string& CoastSpritePath_(std::size_t part, const CoastCornerArt_t& rArt) const;
    void DrawCoastOverlay_(Graphics& rGraphics, const Tile& rTile, const WorldMap& rMap,
                           const TileShape_t& rShape, float shoreShade) const;
    bool TryDrawOccupantPath_(Graphics& rGraphics, const OccupantArt_t& rArt,
                              const std::string& path, const TileShape_t& rShape,
                              const Color_t& tint) const;
    void DrawTerrainLayer_(Graphics& rGraphics, const Tile& rTile, ArtLayer_t layer,
                           const MapAppearance& rAppearance, const TileShape_t& rShape,
                           const TileShape_t& rTerrain) const;
    bool TryDrawLayerSprite_(Graphics& rGraphics, const Tile& rTile,
                             const ImprovementConfig_t& rOccupant,
                             const MapAppearance& rAppearance, const TileShape_t& rShape) const;
    bool TryDrawWaterLandform_(Graphics& rGraphics, const Tile& rTile,
                               const ImprovementConfig_t& rOccupant,
                               const MapAppearance& rAppearance, const TileShape_t& rShape) const;
    void DrawLinkNetworks_(Graphics& rGraphics, const Tile& rTile,
                           const MapAppearance& rAppearance, const TileShape_t& rShape) const;
    void DrawMissingArt_(Graphics& rGraphics, const TileShape_t& rShape) const;
    void DrawObject_(Graphics& rGraphics, const ImprovementConfig_t& rConfig,
                     const std::string& path, const TileShape_t& rShape) const;

    SpriteLibrary& m_rSprites;
    const TileRendererStyle_t& m_rStyle;
    CoastPaths_t m_coastPaths;
};

} // namespace ac
