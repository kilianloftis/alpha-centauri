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
class BaseWorkableAreaDisplay : public UIPanel
{
public:
    // Ratios for positioning and sizing relative to window dimensions
    static constexpr float kCenterXRatio = 0.5f;    // Center at 50% of window width
    static constexpr float kCenterYRatio = 0.5f;    // Center at 50% of window height
    static constexpr float kTileSizeRatio = 0.05f;  // Tile size is 5% of window width

    BaseWorkableAreaDisplay(Graphics& rGraphics, const WorldMap& rWorldMap, const BaseManager* pBase);

    void Render() override;

    // Public accessors for calculated values (used by BaseView for hit testing)
    float GetCenterX() const;
    float GetCenterY() const;
    float GetTileSize() const;

private:
    Graphics& m_rGraphics;
    const WorldMap& m_rWorldMap;
    const BaseManager* m_pBase = nullptr;

    // Helper methods to calculate actual pixel values from ratios
    float GetCenterX_() const;
    float GetCenterY_() const;
    float GetTileSize_() const;

    // Render a single workable tile
    void RenderTile_(const Tile& rTile, float x, float y, float size, bool bIsWorked);
};

} // namespace ac
