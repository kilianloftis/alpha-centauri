#include "ui/world/LocationPanel.h"
#include "game/GameState.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/TileShapeGeometry.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapAppearance.h"
#include <algorithm>
#include <sstream>
#include <string>

namespace ac
{

LocationPanel::LocationPanel(const GameState& rGameState, const TileRenderer& rTileRenderer,
                             WindowLayout_t layout)
    : UIElement(layout)
    , m_rGameState(rGameState)
    , m_rTileRenderer(rTileRenderer)
{
}

void LocationPanel::Render(Graphics& rGraphics)
{
    DrawBackground_(rGraphics);

    if (!m_pSelectedTile)
    {
        DrawEmptyState_(rGraphics);
        return;
    }

    const auto& s = Style().locationPanel;
    const float padding = m_layout.width * s.paddingRatio;
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * s.textFontRatio);
    const float textGap = m_layout.height * s.textGapRatio;
    const float previewSize = std::min(m_layout.width, m_layout.height) * s.previewSizeRatio;
    const float previewX = m_layout.x + (m_layout.width - previewSize) * 0.5f;
    const float previewY = m_layout.y + padding;
    const float textX = m_layout.x + padding;

    const MapAppearance appearance =
        AppearanceOf(m_rGameState.GetWorldMap(), m_rGameState.GetPlayerFaction());
    m_rTileRenderer.Render(rGraphics, *m_pSelectedTile,
                           FlatTileShape(previewX, previewY, previewSize), /*bFogged=*/false,
                           appearance);

    float textY = previewY + previewSize + textGap;
    textY = DrawCoordinates_(rGraphics, textX, textY, fontSize) + textGap;
    textY = DrawElevation_(rGraphics, textX, textY, fontSize) + textGap;
    DrawContents_(rGraphics, appearance, textX, textY, fontSize, textGap);
}

void LocationPanel::DrawBackground_(Graphics& rGraphics) const
{
    const auto& s = Style().locationPanel;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, s.borderColor);
}

void LocationPanel::DrawEmptyState_(Graphics& rGraphics) const
{
    const auto& s = Style().locationPanel;
    const float padding = m_layout.width * s.paddingRatio;
    const unsigned int fontSize =
        static_cast<unsigned int>(m_layout.height * s.textFontRatio);
    rGraphics.DrawText(
        "No tile",
        m_layout.x + padding,
        m_layout.y + padding,
        fontSize,
        s.mutedTextColor);
}

float LocationPanel::DrawCoordinates_(Graphics& rGraphics, float textX, float textY,
                                      unsigned int fontSize) const
{
    std::ostringstream oss;
    oss << "(" << m_pSelectedTile->GetX() << ", " << m_pSelectedTile->GetY() << ")";
    rGraphics.DrawText(oss.str(), textX, textY, fontSize, Style().locationPanel.bodyTextColor);
    return textY + static_cast<float>(fontSize);
}

float LocationPanel::DrawElevation_(Graphics& rGraphics, float textX, float textY,
                                    unsigned int fontSize) const
{
    const int elevation = m_pSelectedTile->GetElevation();
    std::ostringstream oss;
    if (m_pSelectedTile->IsWater())
    {
        oss << "Depth: " << -elevation << "m";
    }
    else
    {
        oss << "Alt: " << elevation << "m";
    }
    rGraphics.DrawText(oss.str(), textX, textY, fontSize, Style().locationPanel.bodyTextColor);
    return textY + static_cast<float>(fontSize);
}

void LocationPanel::DrawContents_(Graphics& rGraphics, const MapAppearance& rAppearance,
                                  float textX, float textY, unsigned int fontSize,
                                  float textGap) const
{
    const auto& s = Style().locationPanel;
    const float lineStep = static_cast<float>(fontSize) + textGap;
    float y = textY;
    const float bottom = m_layout.y + m_layout.height - m_layout.width * s.paddingRatio;

    rAppearance.OccupantsOf(*m_pSelectedTile).ForEach([&](const ImprovementConfig_t& rConfig) {
        if (y + static_cast<float>(fontSize) > bottom)
        {
            return true;
        }
        rGraphics.DrawText(rConfig.name, textX, y, fontSize, s.bodyTextColor);
        y += lineStep;
        return false;
    });
}

} // namespace ac
