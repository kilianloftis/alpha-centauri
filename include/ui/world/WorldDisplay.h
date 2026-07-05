#pragma once

#include "graphics/Graphics.h"
#include "game/map/WorldMap.h"
#include "ui/UIElement.h"
#include <string>
#include <vector>
#include <utility>
#include <optional>

namespace ac
{

using FactionId = int;

class Unit;

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
    explicit WorldDisplay(WindowLayout_t layout);

    // Set the world map to display
    void SetWorldMap(const WorldMap* pWorldMap);

    // Set base info to display bases on the map
    void SetBaseInfo(const std::vector<BaseInfo_t>& baseInfo);

    // Set the unit currently selected by the player (highlighted on the map)
    void SetSelectedUnit(const Unit* pUnit);

    // Set the top-left tile coordinate of the visible viewport
    void SetCameraOffset(int tileX, int tileY);

    float GetEffectiveTileSize() const;
    int GetCameraX() const;
    int GetCameraY() const;
    int GetVisibleCols() const;
    int GetVisibleRows() const;

    // Render the world map using the stored layout.
    void Render(Graphics& rGraphics);

private:
    const WorldMap* m_pWorldMap = nullptr;
    std::vector<BaseInfo_t> m_baseInfo;
    const Unit* m_pSelectedUnit = nullptr;

    WindowLayout_t m_layout;
    float m_tileSize = 0.0f;
    float m_effectiveTileSize = 40.0f;
    int m_visibleCols = 0;
    int m_visibleRows = 0;
    int m_cameraX = 0;
    int m_cameraY = 0;


    // Convert terrain enums to display integers
    int MoistureToInt_(Moisture moisture) const;
    int RockinessToInt_(Rockiness rockiness) const;

    // Render a single tile
    void RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size);

    // Render base markers with owner color and population info
    void RenderBases_(Graphics& rGraphics, int colStart, int rowStart, int colEnd, int rowEnd);

    // Render unit markers on top of bases
    void RenderUnits_(Graphics& rGraphics, int colStart, int rowStart, int colEnd, int rowEnd);
};

} // namespace ac
