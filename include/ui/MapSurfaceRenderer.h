#pragma once

#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/world/FactionBaseArt.h"

#include <functional>
#include <optional>

namespace ac
{

class BaseManager;
class Faction;
class FactionExploredMap;
class Tile;
class WorldMap;
struct BaseSpriteSizesConfig_t;
struct MapOverlayChannelsConfig_t;

// Grid between terrain and objects (SMAC order).
enum class MapGridStyle_t
{
    None,
    // All four edges — sparse clusters (base workable area) need a closed perimeter.
    FullDiamond,
    // NW + NE only so shared edges draw once; ocean/explored rules as on the world map.
    WorldMapEdges,
};

// Shared map-surface paint for WorldDisplay and BaseWorkableAreaDisplay: terrain, grid,
// objects, then bases in a later pass so overhang is not covered by front tiles.
class MapSurfaceRenderer
{
public:
    using YieldLookup_t = TileRenderer::YieldLookup_t;
    using ExploredFn_t = std::function<bool(const Tile&)>;

    MapSurfaceRenderer(const BaseSpriteSizesConfig_t& rBaseSpriteSizes,
                       const MapOverlayChannelsConfig_t& rMapOverlayChannels);

    // One tile's terrain → grid → objects. Shrouded tiles get a fill + grid only.
    void RenderTile(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                    const WorldMap& rMap, bool bFogged, bool bShrouded,
                    const YieldLookup_t& rYieldOf, MapGridStyle_t gridStyle,
                    bool bOceanGrid = false, const ExploredFn_t& rExplored = {});

    // Bare base + building map overlays, seated like TileRenderer object sprites.
    // Call after every RenderTile in the view so front-tile terrain cannot cover overhang.
    // Returns false when no art loads (caller may draw a placeholder).
    bool RenderBase(Graphics& rGraphics, const BaseManager& rBase, const TileShape_t& rShape);

    std::optional<FactionColors_t> ColorsFor(const Faction& rFaction);

private:
    void DrawFullDiamondGrid_(Graphics& rGraphics, const Tile& rTile,
                              const TileShape_t& rShape) const;
    void DrawWorldMapGridEdges_(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                                const WorldMap& rMap, bool bOceanGrid,
                                const ExploredFn_t& rExplored) const;

    FactionBaseArtCache m_art;
    const BaseSpriteSizesConfig_t& m_rBaseSpriteSizes;
    const MapOverlayChannelsConfig_t& m_rMapOverlayChannels;
};

} // namespace ac
