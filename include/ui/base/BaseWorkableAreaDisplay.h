#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include "game/map/Tile.h"

#include <functional>
#include <utility>
#include <vector>

namespace ac
{

// Displays the workable area of a base (21 tiles in 5x5 pattern with corners removed)
// Each tile shows: nutrients minerals energy
// Worked tiles are shown in green
class BaseManager;
class ImprovementRegistry;

class BaseWorkableAreaDisplay : public UIElement
{
public:
    using TileClickCallback_t = std::function<void(const Tile*)>;
    using BaseClickCallback_t = std::function<void()>;

    BaseWorkableAreaDisplay(const BaseManager* pBase,
                            const ImprovementRegistry& rImprovements,
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
    const BaseManager* m_pBase = nullptr;
    const ImprovementRegistry& m_rImprovements;
    TileClickCallback_t m_onTileClicked;
    BaseClickCallback_t m_onBaseClicked;

    float m_tileSize = 0.f;
    float m_startX = 0.f;
    float m_startY = 0.f;
    std::vector<TileRect_t> m_tileRects;

    // Render a single workable tile
    void RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size, bool bIsWorked);
};

} // namespace ac
