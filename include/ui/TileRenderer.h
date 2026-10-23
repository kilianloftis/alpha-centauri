#pragma once

#include "game/base/BaseTypes.h"
#include "game/map/MapUtils.h"
#include "game/display/OccupantArt.h"
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

// Paints one tile as its MapAppearance shows it: a fill, then occupant art by layer, shaded
// as SMAC does (docs/thinker/smac-palette-lighting.md); fog darkens land art under a haze.
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

    // RenderTerrain draws everything up to fog haze on the shape's triangles; RenderObjects draws
    // object art at the corners' mean. Shroud shows only shroud; missing art draws a checker.
    void RenderTerrain(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                       const MapAppearance& rAppearance) const;
    void RenderObjects(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                       const MapAppearance& rAppearance, const YieldLookup_t& rYieldOf) const;

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
    TileShape_t OverlayShape_(const TileShape_t& rShape, const Tile& rTile,
                              TileCover_t cover) const;
    const std::string& CoastSpritePath_(std::size_t part, const CoastCornerArt_t& rArt) const;
    // The elevation gradient, water and land each between their own colours.
    Color_t ElevationFill_(const Tile& rTile) const;
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
