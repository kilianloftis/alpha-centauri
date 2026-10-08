#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/MapSurfaceRenderer.h"
#include "ui/base/BaseDisplaySnapshot.h"

#include <functional>
#include <utility>
#include <vector>

namespace ac
{

// Displays the workable area of a base as a brick of 2:1 diamonds matching the world map:
// lattice neighbors land at map (p − q, p + q). The center diamond is the base tile.
// Tile surfaces (terrain, grid, objects, bases) go through MapSurfaceRenderer — the same
// stack as WorldDisplay. Surrounding tiles then overlay yield text; missing base art keeps
// a BASE placeholder.
class BaseManager;
struct BaseSpriteSizesConfig_t;
struct MapOverlayChannelsConfig_t;

class BaseWorkableAreaDisplay : public UIElement
{
public:
    using TileClickCallback_t = std::function<void(const Tile*)>;
    using BaseClickCallback_t = std::function<void()>;

    BaseWorkableAreaDisplay(const BaseManager& rBase,
                            const BaseDisplaySnapshot_t& rSnapshot,
                            WindowLayout_t layout,
                            const BaseSpriteSizesConfig_t& rBaseSpriteSizes,
                            const MapOverlayChannelsConfig_t& rMapOverlayChannels,
                            TileClickCallback_t onTileClicked,
                            BaseClickCallback_t onBaseClicked);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    struct TileDiamond_t
    {
        TileShape_t shape;
        const Tile* pTile = nullptr;
        // Map delta from the base; higher mapDy is drawn/hit in front.
        int mapDx = 0;
        int mapDy = 0;
        bool bIsBase = false;
    };

    void CacheTileDiamonds_();
    void RenderYieldLabel_(Graphics& rGraphics, const TileDiamond_t& rEntry) const;

    const BaseManager& m_rBase;
    const BaseDisplaySnapshot_t& m_rSnapshot;
    TileClickCallback_t m_onTileClicked;
    BaseClickCallback_t m_onBaseClicked;
    MapSurfaceRenderer m_mapSurface;

    float m_tileWidth = 0.f;
    std::vector<TileDiamond_t> m_tileDiamonds;
};

} // namespace ac
