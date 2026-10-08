#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/TileShapeGeometry.h"
#include "ui/base/BaseDisplaySnapshot.h"

#include <functional>
#include <utility>
#include <vector>

namespace ac
{

// Displays the workable area of a base as a brick of 2:1 diamonds matching the world map:
// lattice neighbors land at map (p − q, p + q). The center diamond is the base tile.
// MapRenderer paints the tiles as the base's faction knows them (no fog) with every base on
// them, the same path as WorldDisplay; surrounding tiles then overlay yield text.
class BaseManager;
class MapRenderer;

class BaseWorkableAreaDisplay : public UIElement
{
public:
    using TileClickCallback_t = std::function<void(const Tile*)>;
    using BaseClickCallback_t = std::function<void()>;

    BaseWorkableAreaDisplay(const BaseManager& rBase,
                            const BaseDisplaySnapshot_t& rSnapshot,
                            WindowLayout_t layout,
                            MapRenderer& rMapRenderer,
                            TileClickCallback_t onTileClicked,
                            BaseClickCallback_t onBaseClicked);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    void PlaceTiles_();
    void RenderYieldLabel_(Graphics& rGraphics, const PlacedTile_t& rPlaced) const;

    const BaseManager& m_rBase;
    const BaseDisplaySnapshot_t& m_rSnapshot;
    MapRenderer& m_rMapRenderer;
    TileClickCallback_t m_onTileClicked;
    BaseClickCallback_t m_onBaseClicked;

    // Back to front: higher map rows draw and hit in front.
    std::vector<PlacedTile_t> m_tiles;
};

} // namespace ac
