#include "ui/commlinks/CommlinksPanel.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "graphics/Graphics.h"
#include "ui/StandingCaptions.h"
#include "ui/style/UiStyle.h"

#include <string>
#include <utility>
#include <vector>

namespace ac
{

CommlinksPanel::CommlinksPanel(GameState& rGameState, const StandingCaptions& rCaptions,
                               WindowLayout_t layout, std::function<void(FactionId_t)> onTalk)
    : UIElement(layout)
    , m_rGameState(rGameState)
    , m_rCaptions(rCaptions)
    , m_onTalk(std::move(onTalk))
{
}

void CommlinksPanel::Render(Graphics& rGraphics)
{
    const FactionId_t playerId = m_rGameState.GetPlayerFaction().GetFactionId();
    DrawFrame_(rGraphics);
    DrawIntegrity_(rGraphics, playerId);
    DrawColumnLabels_(rGraphics);
    DrawRows_(rGraphics, playerId);
}

void CommlinksPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const auto& style = Style().commlinksPanel;
    const float rowsTop = m_layout.y + style.rowStartY * m_layout.height;
    const float row = (static_cast<float>(rEvent.y) - rowsTop) / (style.rowHeight * m_layout.height);
    const std::vector<const Faction*> factions =
        MetFactions_(m_rGameState.GetPlayerFaction().GetFactionId());
    if (rEvent.button != MouseButton_t::Left || row < 0.0f
        || row >= static_cast<float>(factions.size()))
    {
        return;
    }
    const auto index = static_cast<std::size_t>(row);
    m_onTalk(factions[index]->GetFactionId());
}

void CommlinksPanel::DrawFrame_(Graphics& rGraphics) const
{
    const auto& style = Style().commlinksPanel;
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        style.borderColor, Style().outline.width);
    DrawAt_(rGraphics, "Commlinks", style.titlePadX, style.titlePadY, style.titleFontSize,
            style.titleColor);
}

void CommlinksPanel::DrawIntegrity_(Graphics& rGraphics, FactionId_t playerId) const
{
    const auto& style = Style().commlinksPanel;
    const int blemishes = m_rGameState.GetDiplomacyLedger().GetBlemishes(playerId);
    DrawAt_(rGraphics, "Integrity: " + m_rCaptions.Repute(blemishes), style.titlePadX,
            style.integrityPadY, style.rowFontSize, style.statusColor);
}

void CommlinksPanel::DrawColumnLabels_(Graphics& rGraphics) const
{
    const auto& style = Style().commlinksPanel;
    DrawAt_(rGraphics, "Mood", style.moodPadX, style.columnLabelPadY, style.rowFontSize,
            style.titleColor);
    DrawAt_(rGraphics, "Reputation", style.reputePadX, style.columnLabelPadY, style.rowFontSize,
            style.titleColor);
}

std::vector<const Faction*> CommlinksPanel::MetFactions_(FactionId_t playerId) const
{
    const DiplomacyLedger& rLedger = m_rGameState.GetDiplomacyLedger();
    std::vector<const Faction*> factions;
    for (const Faction& rFaction : m_rGameState.Factions())
    {
        const FactionId_t otherId = rFaction.GetFactionId();
        if (otherId != playerId && rLedger.AreKnown(playerId, otherId))
        {
            factions.push_back(&rFaction);
        }
    }
    return factions;
}

void CommlinksPanel::DrawRows_(Graphics& rGraphics, FactionId_t playerId) const
{
    int row = 0;
    for (const Faction* pFaction : MetFactions_(playerId))
    {
        DrawRow_(rGraphics, *pFaction, playerId, row);
        ++row;
    }
}

void CommlinksPanel::DrawRow_(Graphics& rGraphics, const Faction& rFaction, FactionId_t playerId,
                              int row) const
{
    const auto& style = Style().commlinksPanel;
    const DiplomacyLedger& rLedger = m_rGameState.GetDiplomacyLedger();
    const FactionId_t otherId = rFaction.GetFactionId();
    const float padY = style.rowStartY + static_cast<float>(row) * style.rowHeight;
    const DiplomaticStatus_t status = rLedger.GetStatus(playerId, otherId);
    // Its mood toward the player, raw at Vendetta as on SMAC's portrait (terranx-map.md, get_mood).
    const int friction = rLedger.GetFriction(otherId, playerId);
    const int blemishes = rLedger.GetBlemishes(otherId);

    DrawAt_(rGraphics, rFaction.GetDefinition().identity.name, style.rowPadX, padY,
            style.rowFontSize, style.factionNameColor);
    if (status != DiplomaticStatus_t::Neutral)
    {
        DrawAt_(rGraphics, ToString(status), style.statusPadX, padY, style.rowFontSize,
                style.statusColor);
    }
    DrawAt_(rGraphics, m_rCaptions.Mood(friction), style.moodPadX, padY, style.rowFontSize,
            style.statusColor);
    DrawAt_(rGraphics, m_rCaptions.Repute(blemishes), style.reputePadX, padY, style.rowFontSize,
            style.statusColor);
}

void CommlinksPanel::DrawAt_(Graphics& rGraphics, const std::string& rText, float padX,
                             float padY, unsigned int fontSize, const Color_t& color) const
{
    rGraphics.DrawText(rText, m_layout.x + padX * m_layout.width,
                       m_layout.y + padY * m_layout.height, fontSize, color);
}

} // namespace ac
