#include "ui/satellite/OrbitalAttackerPopup.h"

#include "ui/satellite/SatelliteLabeledButton.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

#include <string>

namespace ac
{

OrbitalAttackerPopup::OrbitalAttackerPopup(WindowLayout_t layout,
                                           std::vector<OrbitalAttackerOption_t> attackers,
                                           std::function<void(BuildingId_t)> onConfirm,
                                           std::function<void()> onCancel)
    : UIElement(layout)
    , m_attackers(std::move(attackers))
    , m_onConfirm(std::move(onConfirm))
    , m_onCancel(std::move(onCancel))
{
    RebuildButtons_();
}

void OrbitalAttackerPopup::SelectAttacker_(size_t index)
{
    if (index >= m_attackers.size())
    {
        return;
    }
    if (m_selectedIndex && *m_selectedIndex == index)
    {
        return;
    }
    m_selectedIndex = index;
    RebuildButtons_();
}

void OrbitalAttackerPopup::Confirm_()
{
    if (!m_selectedIndex || !m_onConfirm)
    {
        return;
    }
    const BuildingId_t attackerId = m_attackers[*m_selectedIndex].buildingId;
    m_bShouldClose = true;
    m_onConfirm(attackerId);
}

void OrbitalAttackerPopup::Cancel_()
{
    m_bShouldClose = true;
    if (m_onCancel)
    {
        m_onCancel();
    }
}

void OrbitalAttackerPopup::RebuildButtons_()
{
    m_attackerButtons.clear();
    m_pAttackButton.reset();
    m_pCancelButton.reset();

    const auto& style = Style().satelliteView;
    const WindowLayout_t list = ResolveLayout(m_layout, style.attackerListLayout);
    const float buttonH = style.listButtonHeight;
    const float gap = style.listButtonGap;
    float y = list.y;
    for (size_t i = 0; i < m_attackers.size(); ++i)
    {
        const OrbitalAttackerOption_t& rOpt = m_attackers[i];
        const std::string name = rOpt.pConfig ? rOpt.pConfig->name : rOpt.buildingId;
        const std::string label = name + " (ready: " + std::to_string(rOpt.readyCount)
            + ", " + std::to_string(rOpt.chance) + "%)";
        const bool bSelected = m_selectedIndex && *m_selectedIndex == i;
        m_attackerButtons.push_back(std::make_unique<SatelliteLabeledButton>(
            WindowLayout_t{list.x, y, list.width, buttonH},
            label,
            [this, i]() { SelectAttacker_(i); },
            bSelected));
        y += buttonH + gap;
    }

    m_pAttackButton = std::make_unique<SatelliteLabeledButton>(
        ResolveLayout(m_layout, style.attackerConfirmLayout),
        "Attack",
        [this]() { Confirm_(); },
        /*bSelected*/ false);
    m_pCancelButton = std::make_unique<SatelliteLabeledButton>(
        ResolveLayout(m_layout, style.attackerCancelLayout),
        "Cancel",
        [this]() { Cancel_(); },
        /*bSelected*/ false);
}

void OrbitalAttackerPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().satelliteView;
    const auto& popupStyle = Style().listSelectorPopup;
    const float padding = popupStyle.paddingRatio * m_layout.width;
    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * popupStyle.headerFontSizeRatio);

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText(
        "Select Attacker",
        m_layout.x + padding,
        m_layout.y + padding,
        headerFontSize,
        style.headerColor);

    if (m_attackerButtons.empty())
    {
        rGraphics.DrawText(
            "No ready orbital attackers",
            m_layout.x + padding,
            ResolveLayout(m_layout, style.attackerListLayout).y,
            style.factionFontSize,
            style.cellColor);
    }

    for (const auto& pButton : m_attackerButtons)
    {
        pButton->Render(rGraphics);
    }
    if (m_pAttackButton)
    {
        m_pAttackButton->Render(rGraphics);
    }
    if (m_pCancelButton)
    {
        m_pCancelButton->Render(rGraphics);
    }
}

bool OrbitalAttackerPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        Cancel_();
        return true;
    }
    return false;
}

void OrbitalAttackerPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    const float x = static_cast<float>(rEvent.x);
    const float y = static_cast<float>(rEvent.y);

    if (m_pAttackButton && m_pAttackButton->Contains(x, y))
    {
        m_pAttackButton->HandleMouseClick(rEvent);
        return;
    }
    if (m_pCancelButton && m_pCancelButton->Contains(x, y))
    {
        m_pCancelButton->HandleMouseClick(rEvent);
        return;
    }
    for (auto& pButton : m_attackerButtons)
    {
        if (pButton->Contains(x, y))
        {
            pButton->HandleMouseClick(rEvent);
            return;
        }
    }

    if (!Contains(x, y))
    {
        Cancel_();
    }
}

} // namespace ac
