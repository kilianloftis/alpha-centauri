#include "ui/unit-designer/DesignStatsDisplay.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitComponentConfig.h"
#include "game/effects/EffectEnums.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"
#include <sstream>

namespace ac
{

namespace
{

constexpr int k_NoFuel = 0;

} // namespace

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
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().designStatsDisplay.backgroundColor
    );
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().designStatsDisplay.borderColor
    );

    const float padding = m_layout.height * Style().designStatsDisplay.paddingRatio;
    const unsigned int headerSize = static_cast<unsigned int>(
        m_layout.height * Style().designStatsDisplay.headerFontSizeRatio);
    const unsigned int statSize = static_cast<unsigned int>(
        m_layout.height * Style().designStatsDisplay.statFontSizeRatio);
    const float lineH = m_layout.height * Style().designStatsDisplay.lineHeightRatio;

    rGraphics.DrawText(
        "Design Stats",
        m_layout.x + padding,
        m_layout.y + padding,
        headerSize,
        Style().designStatsDisplay.headerColor
    );

    if (!m_pState->HasAllMandatory(*m_pSlots))
    {
        rGraphics.DrawText(
            "Fill all required slots",
            m_layout.x + padding,
            m_layout.y + padding + lineH,
            statSize,
            Style().designStatsDisplay.incompleteTextColor
        );
        return;
    }

    const UnitDesign design(*m_pSlots, m_pState->components);

    float y = m_layout.y + padding + lineH;
    auto drawStat = [&](const std::string& rLabel, int value)
    {
        std::ostringstream oss;
        oss << rLabel << ": " << value;
        rGraphics.DrawText(oss.str(), m_layout.x + padding, y, statSize,
                           Style().designStatsDisplay.statTextColor);
        y += lineH;
    };

    drawStat("Attack",   design.GetStat(StatId_t::Attack));
    drawStat("Defense",  design.GetStat(StatId_t::Defense));
    drawStat("Movement", design.GetMovementPoints());
    drawStat("HP",       design.GetStat(StatId_t::HitPoints));
    if (design.GetStat(StatId_t::Fuel) > k_NoFuel)
    {
        drawStat("Fuel", design.GetStat(StatId_t::Fuel));
    }
    drawStat("Cost", design.GetBaseCost());

    const float saveButtonH = m_layout.height * Style().designStatsDisplay.saveButtonHeightRatio;
    const float saveButtonY = m_layout.y + m_layout.height - saveButtonH - padding;
    m_saveButtonRect = {
        m_layout.x + padding,
        saveButtonY,
        m_layout.width - padding * Style().designStatsDisplay.horizontalPaddingMultiplier,
        saveButtonH
    };

    rGraphics.DrawFilledRect(
        m_saveButtonRect.x, m_saveButtonRect.y,
        m_saveButtonRect.width, m_saveButtonRect.height,
        Style().designStatsDisplay.saveButtonFillColor
    );
    rGraphics.DrawRect(
        m_saveButtonRect.x, m_saveButtonRect.y,
        m_saveButtonRect.width, m_saveButtonRect.height,
        Style().designStatsDisplay.saveButtonBorderColor
    );
    rGraphics.DrawText(
        "Save Design",
        m_saveButtonRect.x + padding,
        m_saveButtonRect.y + saveButtonH * Style().designStatsDisplay.saveButtonTextYOffsetRatio,
        statSize,
        Style().designStatsDisplay.saveButtonTextColor
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
