#include "ui/unit-designer/DesignStatsDisplay.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitComponentConfig.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include <sstream>

namespace ac
{

DesignStatsDisplay::DesignStatsDisplay(
    const UnitDesignerState_t* pState,
    const std::vector<UnitSlotConfig_t>* pSlots,
    WindowLayout_t layout,
    std::function<void()> onSaveDesign
)
    : UIElement(layout)
    , m_pState(pState)
    , m_pSlots(pSlots)
    , m_onSaveDesign(std::move(onSaveDesign))
{}

void DesignStatsDisplay::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{15, 15, 25, 255});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{60, 60, 110, 255});

    const float padding           = m_layout.height * k_PaddingRatio;
    const unsigned int headerSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int statSize   = static_cast<unsigned int>(m_layout.height * k_StatFontSizeRatio);
    const float lineH             = m_layout.height * k_LineHeightRatio;

    rGraphics.DrawText("Design Stats", m_layout.x + padding, m_layout.y + padding, headerSize, Color::Yellow());

    if (!m_pState->HasAllMandatory(*m_pSlots))
    {
        rGraphics.DrawText(
            "Fill all required slots",
            m_layout.x + padding,
            m_layout.y + padding + lineH,
            statSize,
            Color{120, 120, 120, 255}
        );
        return;
    }

    const UnitDesign design(*m_pSlots, m_pState->components);

    float y = m_layout.y + padding + lineH;
    auto drawStat = [&](const std::string& rLabel, int value)
    {
        std::ostringstream oss;
        oss << rLabel << ": " << value;
        rGraphics.DrawText(oss.str(), m_layout.x + padding, y, statSize);
        y += lineH;
    };

    drawStat("Attack",   design.GetAttack());
    drawStat("Defense",  design.GetDefense());
    drawStat("Movement", design.GetMovement());
    drawStat("HP",       design.GetHitPoints());
    if (design.GetFuel() > 0)
    {
        drawStat("Fuel", design.GetFuel());
    }
    drawStat("Cost", design.GetBaseCost());

    const float saveButtonH = m_layout.height * k_SaveButtonHeightRatio;
    const float saveButtonY = m_layout.y + m_layout.height - saveButtonH - padding;
    m_saveButtonRect = {m_layout.x + padding, saveButtonY, m_layout.width - padding * 2.0f, saveButtonH};

    rGraphics.DrawFilledRect(
        m_saveButtonRect.x, m_saveButtonRect.y,
        m_saveButtonRect.width, m_saveButtonRect.height,
        Color{30, 60, 30, 255}
    );
    rGraphics.DrawRect(
        m_saveButtonRect.x, m_saveButtonRect.y,
        m_saveButtonRect.width, m_saveButtonRect.height,
        Color::Green()
    );
    rGraphics.DrawText(
        "Save Design",
        m_saveButtonRect.x + padding,
        m_saveButtonRect.y + saveButtonH * 0.2f,
        statSize,
        Color::Green()
    );
}

void DesignStatsDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left || !m_pState->HasAllMandatory(*m_pSlots))
    {
        return;
    }

    if (ContainsMouseCoord(m_saveButtonRect, rEvent) && m_onSaveDesign)
    {
        m_onSaveDesign();
    }
}

} // namespace ac
