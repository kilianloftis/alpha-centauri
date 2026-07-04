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

static constexpr float k_DefaultTileScale = 1.0f / 10.0f;

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
    WorldDisplay() = default;

    // Set the world map to display
    void SetWorldMap(const WorldMap* pWorldMap);

    // Set base info to display bases on the map
    void SetBaseInfo(const std::vector<BaseInfo_t>& baseInfo);

    // Set the pixel size of each tile. Pass 0 to restore the window-size-based default.
    void SetTileSize(float tileSize);

    // Set the top-left tile coordinate of the visible viewport
    void SetCameraOffset(int tileX, int tileY);

    float GetTileSize() const;
    float GetEffectiveTileSize() const;
    int GetCameraX() const;
    int GetCameraY() const;

    // Render the world map showing only the tiles that fit within [x,y,w,h]
    // using the current tile size and camera offset.
    void Render(Graphics& rGraphics, float x, float y, float w, float h);

private:
    const WorldMap* m_pWorldMap = nullptr;
    const Graphics* m_pGraphics = nullptr;
    std::vector<BaseInfo_t> m_baseInfo;

    float m_tileSize = 0.0f;
    int m_cameraX = 0;
    int m_cameraY = 0;

    // Convert terrain enums to display integers
    int MoistureToInt_(Moisture moisture) const;
    int RockinessToInt_(Rockiness rockiness) const;

    // Render a single tile
    void RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size);

    // Render base markers with owner color and population info
    void RenderBases_(Graphics& rGraphics, float originX, float originY, float tileSize, int camX, int camY, int camXEnd, int camYEnd);
};

} // namespace ac
