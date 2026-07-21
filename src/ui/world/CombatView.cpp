#include "ui/world/CombatView.h"

#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"
#include "ui/world/InfoPanelElement.h"
#include "ui/world/WorldDisplay.h"

#include <optional>
#include <string>
#include <vector>

namespace ac
{

namespace
{

std::string FormatHpLine_(const std::string& label, int hp)
{
    return label + ": " + std::to_string(hp) + " HP";
}

std::string SideLabel_(CombatSide_t side)
{
    return side == CombatSide_t::Attacker ? "Attacker" : "Defender";
}

} // namespace

CombatView::CombatView(WindowLayout_t layout,
                       CombatResult_t result,
                       const Tile& rAttackerTile,
                       const Tile& rDefenderTile,
                       std::string attackerName,
                       std::string defenderName,
                       WorldDisplay& rWorldDisplay,
                       WindowLayout_t mapLayout,
                       std::function<void()> onFinished)
    : IGameView(layout)
    , m_rWorldDisplay(rWorldDisplay)
    , m_mapLayout(mapLayout)
    , m_onFinished(std::move(onFinished))
    , m_attackerName(std::move(attackerName))
    , m_defenderName(std::move(defenderName))
{
    auto pAttacker = std::make_unique<InfoPanelElement>(ResolveLayout(m_layout, Style().layouts.leftPanel));
    m_pAttackerPanel = pAttacker.get();
    m_elements.push_back(std::move(pAttacker));

    auto pRound = std::make_unique<InfoPanelElement>(ResolveLayout(m_layout, Style().layouts.bottomPanel));
    m_pRoundPanel = pRound.get();
    m_elements.push_back(std::move(pRound));

    auto pDefender = std::make_unique<InfoPanelElement>(ResolveLayout(m_layout, Style().layouts.rightPanel));
    m_pDefenderPanel = pDefender.get();
    m_elements.push_back(std::move(pDefender));

    m_presentation.Begin(result, rAttackerTile, rDefenderTile);
    RefreshPanels_();

    if (!m_presentation.IsActive())
    {
        m_bShouldClose = true;
    }
}

void CombatView::Render(Graphics& rGraphics)
{
    m_presentation.Update();
    RefreshPanels_();
    m_presentation.Render(rGraphics, m_rWorldDisplay, m_mapLayout);
    IGameView::Render(rGraphics);
    FinishIfDone_();
}

bool CombatView::HandleKey(const KeyEvent_t& /*rEvent*/)
{
    // Combat playback is non-interactive: swallow keys so global shortcuts cannot open
    // other overlays and WorldView cannot end the turn.
    return true;
}

void CombatView::HandleMouse(const MouseEvent_t& /*rEvent*/)
{
}

void CombatView::OnPopped()
{
    if (!m_bFinishedNotified && m_onFinished)
    {
        m_bFinishedNotified = true;
        m_onFinished();
    }
}

void CombatView::RefreshPanels_()
{
    const auto& s = Style().combatView;
    const CombatResult_t& rResult = m_presentation.GetResult();
    const CombatRound_t* pRound = m_presentation.GetDisplayedRound();

    const int attackerHp = pRound ? pRound->attackerHpAfter
                                  : (rResult.rounds.empty() ? 0 : rResult.rounds.back().attackerHpAfter);
    const int defenderHp = pRound ? pRound->defenderHpAfter
                                  : (rResult.rounds.empty() ? 0 : rResult.rounds.back().defenderHpAfter);

    m_pAttackerPanel->SetInfoLines({
        {m_attackerName.empty() ? "Attacker" : m_attackerName, s.sideNameColor},
        {FormatHpLine_("HP", attackerHp), s.hpLineColor},
    });

    m_pDefenderPanel->SetInfoLines({
        {m_defenderName.empty() ? "Defender" : m_defenderName, s.sideNameColor},
        {FormatHpLine_("HP", defenderHp), s.hpLineColor},
    });

    std::vector<InfoPanelElement::InfoLine> roundLines;
    if (const std::optional<size_t> roundIndex = m_presentation.GetDisplayedRoundIndex())
    {
        roundLines.push_back({
            "Round " + std::to_string(*roundIndex + 1) + " / "
                + std::to_string(rResult.rounds.size()),
            s.roundHeaderColor});
        if (pRound)
        {
            roundLines.push_back({
                "Rolls " + std::to_string(pRound->attackRoll) + " vs "
                    + std::to_string(pRound->defenseRoll),
                s.rollsLineColor});
            roundLines.push_back({
                SideLabel_(pRound->roundWinner) + " hits for "
                    + std::to_string(pRound->damage),
                s.hitLineColor});
        }
    }
    else
    {
        roundLines.push_back({"Combat", s.idleLabelColor});
    }
    m_pRoundPanel->SetInfoLines(roundLines);
}

void CombatView::FinishIfDone_()
{
    if (m_presentation.IsActive() || m_bShouldClose)
    {
        return;
    }
    m_bShouldClose = true;
}

} // namespace ac
