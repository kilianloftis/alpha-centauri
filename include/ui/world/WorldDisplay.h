#pragma once

#include "graphics/Graphics.h"
#include "game/map/WorldMap.h"
#include <string>
#include <vector>
#include <utility>
#include <optional>

namespace ac
{

using FactionId = int;

struct BaseInfo_t
{
    int x;
    int y;
    std::string name;              // Base name
    FactionId factionId;           // Current owner
    std::optional<FactionId> previousFactionId;  // For capture animations/history
    int populationSize;            // Current base population
};

// Displays the world map as a grid of tiles
// Each tile shows: moisture rockiness elevation(km)
class WorldDisplay
{
public:
    WorldDisplay(Graphics& rGraphics);

    // Set the world map to display
    void SetWorldMap(const WorldMap* pWorldMap);

    // Set base info to display bases on the map
    void SetBaseInfo(const std::vector<BaseInfo_t>& baseInfo);

    // Render the world map grid at specified position
    // tileSize: pixel size of each tile
    void Render(float x, float y, float tileSize = 40.0f);

    // Render the world map grid filling the given bounds, auto-computing tile size
    void Render(float x, float y, float w, float h);

private:
    Graphics& m_rGraphics;
    const WorldMap* m_pWorldMap = nullptr;
    std::vector<BaseInfo_t> m_baseInfo;

    // Convert terrain enums to display integers
    int MoistureToInt_(Moisture moisture) const;
    int RockinessToInt_(Rockiness rockiness) const;

    // Render a single tile
    void RenderTile_(const Tile& rTile, float x, float y, float size);

    // Render base markers with owner color and population info
    void RenderBases_(float originX, float originY, float tileSize);
};

} // namespace ac
