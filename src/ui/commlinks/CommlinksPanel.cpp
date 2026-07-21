#include "ui/commlinks/CommlinksPanel.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "graphics/Graphics.h"

#include <string>

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor {30, 30, 50, 255};
constexpr Color_t k_BorderColor     {80, 80, 120, 255};
constexpr unsigned int k_TitleFontSize = 18;
constexpr unsigned int k_RowFontSize   = 16;
constexpr float k_TitlePadX            = 0.05f;
constexpr float k_TitlePadY            = 0.05f;
constexpr float k_RowStartY            = 0.20f;
constexpr float k_RowHeight            = 0.08f;
constexpr float k_RowPadX              = 0.05f;
constexpr float k_StatusPadX           = 0.55f;

} // namespace

CommlinksPanel::CommlinksPanel(GameState& rGameState, WindowLayout_t layout)
    : UIElement(layout)
    , m_rGameState(rGameState)
{
}

void CommlinksPanel::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);

    rGraphics.DrawText(
        "Commlinks",
        m_layout.x + k_TitlePadX * m_layout.width,
        m_layout.y + k_TitlePadY * m_layout.height,
        k_TitleFontSize,
        Color_t::White());

    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pPlayer)
    {
        return;
    }

    const DiplomacyLedger& rLedger = m_rGameState.GetDiplomacyLedger();
    const FactionId_t playerId = pPlayer->GetFactionId();
    int row = 0;

    for (const Faction& rFaction : m_rGameState.Factions())
    {
        const FactionId_t otherId = rFaction.GetFactionId();
        if (otherId == playerId || !rLedger.AreKnown(playerId, otherId))
        {
            continue;
        }

        const float rowY = m_layout.y + (k_RowStartY + static_cast<float>(row) * k_RowHeight) * m_layout.height;
        const std::string& rName = rFaction.GetDefinition().identity.name;
        const std::string status = ToString(rLedger.GetStatus(playerId, otherId));

        rGraphics.DrawText(
            rName,
            m_layout.x + k_RowPadX * m_layout.width,
            rowY,
            k_RowFontSize,
            Color_t::Yellow());

        if (!status.empty())
        {
            rGraphics.DrawText(
                status,
                m_layout.x + k_StatusPadX * m_layout.width,
                rowY,
                k_RowFontSize,
                Color_t::White());
        }

        ++row;
    }
}

} // namespace ac
