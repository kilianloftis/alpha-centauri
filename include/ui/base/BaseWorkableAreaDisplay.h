#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include "game/map/Tile.h"

#include <functional>
#include <utility>

namespace ac
{

// Displays the workable area of a base (21 tiles in 5x5 pattern with corners removed)
// Each tile shows: nutrients minerals energy
// Worked tiles are shown in green
class BaseManager;

class BaseWorkableAreaDisplay : public UIElement
{
public:
    using TileClickCallback_t = std::function<void(int tileX, int tileY)>;

    BaseWorkableAreaDisplay(const BaseManager* pBase, WindowLayout_t layout, TileClickCallback_t onTileClicked);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    float GetTileSize_() const;
    const BaseManager* m_pBase = nullptr;
    TileClickCallback_t m_onTileClicked;

    // Render a single workable tile
    void RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size, bool bIsWorked);
};

} // namespace ac
