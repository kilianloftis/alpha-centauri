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
class BaseWorkableAreaDisplay : public IBasePanel
{
public:
    static constexpr float kBaseAreaCenterX = 400.f;
    static constexpr float kBaseAreaCenterY = 300.f;
    static constexpr float kBaseTileSize = 50.f;

    BaseWorkableAreaDisplay(Graphics& rGraphics, const WorldMap& rWorldMap);

    // Set the base to display workable area for
    void SetBase(const BaseManager* pBase);

    // IBasePanel: renders at the fixed base area position
    void Render(Graphics& rGraphics) override;

    // Render the workable area at a specified position
    // tileSize: pixel size of each tile
    void Render(float x, float y, float tileSize = 45.0f);

private:
    Graphics& m_rGraphics;
    const WorldMap& m_rWorldMap;
    const BaseManager* m_pBase = nullptr;

    // Render a single workable tile
    void RenderTile_(const Tile& rTile, float x, float y, float size, bool bIsWorked);
};

} // namespace ac
