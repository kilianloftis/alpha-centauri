#pragma once

#include "ui/UIElement.h"

namespace ac
{

class GameState;
class Graphics;
class MapAppearance;
class MapRenderer;
class Tile;

// Dashboard location column: preview of the selected map tile, drawn by MapRenderer as the
// player knows it (no fog), plus feature names from ImprovementConfig_t::name (terrain +
// improvements in improvements.json).
class LocationPanel : public UIElement
{
public:
    LocationPanel(const GameState& rGameState, MapRenderer& rMapRenderer, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

    void SetSelectedTile(const Tile* pTile) { m_pSelectedTile = pTile; }
    const Tile* GetSelectedTile() const { return m_pSelectedTile; }

private:
    void DrawBackground_(Graphics& rGraphics) const;
    void DrawEmptyState_(Graphics& rGraphics) const;
    float DrawCoordinates_(Graphics& rGraphics, float textX, float textY, unsigned int fontSize) const;
    float DrawElevation_(Graphics& rGraphics, float textX, float textY, unsigned int fontSize) const;
    void DrawContents_(Graphics& rGraphics, const MapAppearance& rAppearance, float textX,
                       float textY, unsigned int fontSize, float textGap) const;

    const GameState& m_rGameState;
    MapRenderer& m_rMapRenderer;
    const Tile* m_pSelectedTile = nullptr;
};

} // namespace ac
