#pragma once

#include "game/units/Unit.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/TileShapeGeometry.h"
#include "ui/UIElement.h"
#include "ui/world/FactionBaseArt.h"

#include <functional>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace ac
{

class BaseManager;
class GameState;
class MapAppearance;
class SpriteLibrary;
class Tile;
struct BaseSpriteSizesConfig_t;
struct MapOverlayChannelsConfig_t;
struct MapRendererStyle_t;
struct TileRendererStyle_t;

// Which bases and units a display shows on its tiles; an empty filter shows none.
struct MapContent_t
{
    std::function<bool(const BaseManager&)> showsBase;
    std::function<bool(const Unit&)> showsUnit;
    // Drawn with the selection border.
    const Unit* pSelectedUnit = nullptr;
};

// Where Render drew each unit's marker.
using UnitMarkerRects_t = std::unordered_map<UnitId_t, Rectangle_t>;

// The one path map tiles take to the screen: the world map, a base's workable area, the location
// preview, and the minimap's colours. A display passes the tiles it placed (back to front), the
// MapAppearance of the viewer it shows, and the content it shows on them. Render draws in SMAC's
// order: each tile's terrain, grid edges and objects; then each tile's base, art and name, so
// nothing in front covers its overhang; then one unit marker per tile (none on a drawn base
// unless that unit is selected).
class MapRenderer
{
public:
    MapRenderer(SpriteLibrary& rSprites, const GameState& rGameState,
                const TileRendererStyle_t& rTileStyle, const MapRendererStyle_t& rStyle);

    // Returns where each drawn unit's marker landed.
    UnitMarkerRects_t Render(Graphics& rGraphics, std::span<const PlacedTile_t> tiles,
                             const MapAppearance& rAppearance, const MapContent_t& rContent);

    // The tile's one colour, as the minimap paints it.
    Color_t TileColor(const Tile& rTile, const MapAppearance& rAppearance) const;

private:
    using PlacedSet_t = std::unordered_set<const Tile*>;

    void DrawGrid_(Graphics& rGraphics, const PlacedTile_t& rPlaced, const PlacedSet_t& rPlacedSet,
                   const MapAppearance& rAppearance) const;
    void DrawBase_(Graphics& rGraphics, const PlacedTile_t& rPlaced,
                   const MapAppearance& rAppearance, const MapContent_t& rContent);
    void DrawBaseArt_(Graphics& rGraphics, const BaseManager& rBase, const TileShape_t& rShape);
    void DrawBaseName_(Graphics& rGraphics, const BaseManager& rBase, const TileShape_t& rShape);
    void DrawUnits_(Graphics& rGraphics, const PlacedTile_t& rPlaced,
                    const MapAppearance& rAppearance, const MapContent_t& rContent,
                    UnitMarkerRects_t& rMarkers) const;

    const GameState& m_rGameState;
    const MapRendererStyle_t& m_rStyle;
    const BaseSpriteSizesConfig_t& m_rBaseSpriteSizes;
    const MapOverlayChannelsConfig_t& m_rMapOverlayChannels;
    TileRenderer m_tiles;
    FactionBaseArtCache m_baseArt;
    TileRenderer::YieldLookup_t m_yieldOf;
};

} // namespace ac
