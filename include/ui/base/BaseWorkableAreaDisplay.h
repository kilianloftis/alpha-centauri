#pragma once

#include "ui/UIElement.h"
#include "input/Input.h"
#include "game/map/Tile.h"

#include <optional>
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
    static constexpr float kTileSizeRatio = 0.05f;  // Tile size is 5% of window width

    BaseWorkableAreaDisplay(const BaseManager* pBase, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;
    void HandleMouse(const MouseEvent_t& rEvent);

    float GetTileSize() const;

private:
    const BaseManager* m_pBase = nullptr;
    std::optional<std::pair<int, int>> m_lastClickedTile;

    // Render a single workable tile
    void RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size, bool bIsWorked);
};

} // namespace ac
