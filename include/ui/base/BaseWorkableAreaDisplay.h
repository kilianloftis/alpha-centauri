#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include "game/map/Tile.h"
#include "ui/base/BaseDisplaySnapshot.h"

#include <functional>
#include <utility>
#include <vector>

namespace ac
{

// Displays the workable area of a base: the 20-tile ring in a 5x5 pattern with the corners
// removed. The center is the base's own tile and is drawn separately.
// Each tile shows: nutrients minerals energy. Worked tiles are shown in green.
class BaseManager;

class BaseWorkableAreaDisplay : public UIElement
{
public:
    using TileClickCallback_t = std::function<void(const Tile*)>;
    using BaseClickCallback_t = std::function<void()>;

    BaseWorkableAreaDisplay(const BaseManager& rBase,
                            const BaseDisplaySnapshot_t& rSnapshot,
                            WindowLayout_t layout,
                            TileClickCallback_t onTileClicked,
                            BaseClickCallback_t onBaseClicked);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    struct TileRect_t
    {
        Rectangle_t rect;
        const Tile* pTile;
    };

    void CacheTileRects_();
    const BaseManager& m_rBase;
    const BaseDisplaySnapshot_t& m_rSnapshot;
    TileClickCallback_t m_onTileClicked;
    BaseClickCallback_t m_onBaseClicked;

    float m_tileSize = 0.f;
    float m_startX = 0.f;
    float m_startY = 0.f;
    std::vector<TileRect_t> m_tileRects;

    // Render a single workable tile
    void RenderTile_(Graphics& rGraphics, float x, float y, float size,
                     const TileDisplay_t& rTile);
};

} // namespace ac
