#pragma once

#include "ui/base/IBasePanel.h"
#include "graphics/Graphics.h"
#include "game/map/WorldMap.h"
#include "game/faction/base/BaseManager.h"

namespace ac
{

// Displays the workable area of a base (21 tiles in 5x5 pattern with corners removed)
// Each tile shows: nutrients minerals energy
// Worked tiles are shown in green
class BaseWorkableAreaDisplay : public UIElement
{
public:
    static constexpr float kTileSizeRatio = 0.05f;  // Tile size is 5% of window width

    BaseWorkableAreaDisplay(Graphics& rGraphics, const WorldMap& rWorldMap, const BaseManager* pBase, ResolvedLayout_t layout);

    void Render() override;

    float GetTileSize() const;

private:
    const BaseManager* m_pBase = nullptr;

    // Render a single workable tile
    void RenderTile_(const Tile& rTile, float x, float y, float size, bool bIsWorked);
};

} // namespace ac
