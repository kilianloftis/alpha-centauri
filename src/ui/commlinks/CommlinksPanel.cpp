#include "ui/commlinks/CommlinksPanel.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <string>

namespace ac
{

CommlinksPanel::CommlinksPanel(GameState& rGameState, WindowLayout_t layout)
    : UIElement(layout)
    , m_rGameState(rGameState)
{
}

void CommlinksPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().commlinksPanel;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText(
        "Commlinks",
        m_layout.x + style.titlePadX * m_layout.width,
        m_layout.y + style.titlePadY * m_layout.height,
        style.titleFontSize,
        style.titleColor);

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

        const float rowY = m_layout.y + (style.rowStartY + static_cast<float>(row) * style.rowHeight) * m_layout.height;
        const std::string& rName = rFaction.GetDefinition().identity.name;
        const std::string status = ToString(rLedger.GetStatus(playerId, otherId));

        rGraphics.DrawText(
            rName,
            m_layout.x + style.rowPadX * m_layout.width,
            rowY,
            style.rowFontSize,
            style.factionNameColor);

        if (!status.empty())
        {
            rGraphics.DrawText(
                status,
                m_layout.x + style.statusPadX * m_layout.width,
                rowY,
                style.rowFontSize,
                style.statusColor);
        }

        ++row;
    }
}

} // namespace ac
