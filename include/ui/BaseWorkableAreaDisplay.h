#pragma once

#include "graphics/Graphics.h"
#include "game/map/WorldMap.h"

namespace ac
{

class Base;

// Displays the workable area of a base (21 tiles in 5x5 pattern with corners removed)
// Each tile shows: nutrients minerals energy
// Worked tiles are shown in green
class BaseWorkableAreaDisplay
{
public:
    BaseWorkableAreaDisplay(Graphics& rGraphics, const WorldMap& rWorldMap);

    // Set the base to display workable area for
    void SetBase(const Base* pBase);

    // Render the workable area at specified position
    // tileSize: pixel size of each tile
    void Render(float x, float y, float tileSize = 45.0f);

private:
    Graphics& m_rGraphics;
    const WorldMap& m_rWorldMap;
    const Base* m_pBase = nullptr;

    // Render a single workable tile
    void RenderTile_(const Tile& rTile, float x, float y, float size, bool bIsWorked);
};

} // namespace ac
