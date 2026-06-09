#pragma once

#include "graphics/Graphics.h"
#include "game/map/WorldMap.h"
#include <vector>
#include <utility>

namespace ac
{

// Displays the world map as a grid of tiles
// Each tile shows: moisture rockiness elevation(km)
class WorldDisplay
{
public:
    WorldDisplay(Graphics& rGraphics);

    // Set the world map to display
    void SetWorldMap(const WorldMap* pWorldMap);

    // Set base positions to display "BASE" labels on the map
    void SetBasePositions(const std::vector<std::pair<int, int>>& basePositions);

    // Render the world map grid at specified position
    // tileSize: pixel size of each tile
    void Render(float x, float y, float tileSize = 40.0f);

private:
    Graphics& m_rGraphics;
    const WorldMap* m_pWorldMap = nullptr;
    std::vector<std::pair<int, int>> m_basePositions;

    // Convert terrain enums to display integers
    int MoistureToInt_(Moisture moisture) const;
    int RockinessToInt_(Rockiness rockiness) const;

    // Render a single tile
    void RenderTile_(const Tile& rTile, float x, float y, float size);
};

} // namespace ac
