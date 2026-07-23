#include "ui/world/MinimapDisplay.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"

#include <algorithm>

namespace ac
{

namespace
{

struct PlayerFogMaps_t
{
    const FactionExploredMap* explored = nullptr;
    const FactionVisibleMap* visible = nullptr;
};

PlayerFogMaps_t PlayerFog_(const GameState& rGameState)
{
    const Faction* pPlayer = rGameState.GetPlayerFaction();
    if (!pPlayer || !pPlayer->GetExploredMap().IsSized() || !pPlayer->GetVisibleMap().IsSized())
    {
        return {};
    }
    return {&pPlayer->GetExploredMap(), &pPlayer->GetVisibleMap()};
}

} // namespace

MinimapDisplay::MinimapDisplay(const GameState& rGameState, WindowLayout_t layout)
    : UIElement(layout)
    , m_rGameState(rGameState)
{
}

void MinimapDisplay::Render(Graphics& rGraphics)
{
    const WorldMap& rWorldMap = m_rGameState.GetWorldMap();
    const int mapWidth = rWorldMap.GetWidth();
    const int mapHeight = rWorldMap.GetHeight();
    if (mapWidth <= 0 || mapHeight <= 0)
    {
        return;
    }

    // Fit the whole map into the panel while preserving aspect ratio (letterbox).
    const float scale = std::min(m_layout.width / static_cast<float>(mapWidth),
                                 m_layout.height / static_cast<float>(mapHeight));
    const float tileSize = scale;
    const float mapPixelW = tileSize * static_cast<float>(mapWidth);
    const float mapPixelH = tileSize * static_cast<float>(mapHeight);
    const float originX = m_layout.x + (m_layout.width - mapPixelW) * 0.5f;
    const float originY = m_layout.y + (m_layout.height - mapPixelH) * 0.5f;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             Style().worldDisplay.shroudColor);

    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    for (int row = 0; row < mapHeight; ++row)
    {
        for (int col = 0; col < mapWidth; ++col)
        {
            const Tile* pTile = rWorldMap.GetTile(col, row);
            if (!pTile)
            {
                continue;
            }

            const float tileX = originX + static_cast<float>(col) * tileSize;
            const float tileY = originY + static_cast<float>(row) * tileSize;

            if (fog.explored && !fog.explored->IsExplored(*pTile))
            {
                // Already filled with shroud via the panel background.
                continue;
            }

            const bool bFogged = fog.visible && !fog.visible->IsVisible(*pTile);
            rGraphics.DrawFilledRect(tileX, tileY, tileSize, tileSize,
                                     TileRenderer::FillColor(*pTile, bFogged));
        }
    }
}

} // namespace ac
