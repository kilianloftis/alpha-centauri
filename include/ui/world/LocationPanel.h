#pragma once

#include "ui/UIElement.h"

namespace ac
{

class GameState;
class Graphics;
class MapAppearance;
class MapRenderer;
class SpriteLibrary;
class Tile;

// The selected tile's preview, as the player knows it, with its feature names.
class LocationPanel : public UIElement
{
public:
    LocationPanel(const GameState& rGameState, MapRenderer& rMapRenderer, SpriteLibrary& rSprites,
                  WindowLayout_t layout);

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
    SpriteLibrary& m_rSprites;
    const Tile* m_pSelectedTile = nullptr;
};

} // namespace ac
