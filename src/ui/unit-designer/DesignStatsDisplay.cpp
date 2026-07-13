#include "ui/unit-designer/DesignStatsDisplay.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitComponentConfig.h"
#include "game/effects/EffectEnums.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include <sstream>

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor          {15, 15, 25, 255};
constexpr Color_t k_BorderColor              {60, 60, 110, 255};
constexpr Color_t k_IncompleteTextColor      {120, 120, 120, 255};
constexpr Color_t k_SaveButtonFillColor      {30, 60, 30, 255};
constexpr float k_HeaderFontSizeRatio      = 0.04f;
constexpr float k_StatFontSizeRatio        = 0.032f;
constexpr float k_LineHeightRatio          = 0.055f;
constexpr float k_PaddingRatio             = 0.02f;
constexpr float k_SaveButtonHeightRatio    = 0.07f;
constexpr float k_SaveButtonTextYOffsetRatio = 0.2f;
constexpr float k_HorizontalPaddingMultiplier = 2.0f;
constexpr int   k_NoFuel                   = 0;

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
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);

    const float padding           = m_layout.height * k_PaddingRatio;
    const unsigned int headerSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int statSize   = static_cast<unsigned int>(m_layout.height * k_StatFontSizeRatio);
    const float lineH             = m_layout.height * k_LineHeightRatio;

    rGraphics.DrawText("Design Stats", m_layout.x + padding, m_layout.y + padding, headerSize, Color_t::Yellow());

    if (!m_pState->HasAllMandatory(*m_pSlots))
    {
        rGraphics.DrawText(
            "Fill all required slots",
            m_layout.x + padding,
            m_layout.y + padding + lineH,
            statSize,
            k_IncompleteTextColor
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

    drawStat("Attack",   design.GetStat(StatId_t::Attack));
    drawStat("Defense",  design.GetStat(StatId_t::Defense));
    drawStat("Movement", design.GetMovementPoints());
    drawStat("HP",       design.GetStat(StatId_t::HitPoints));
    if (design.GetStat(StatId_t::Fuel) > k_NoFuel)
    {
        drawStat("Fuel", design.GetStat(StatId_t::Fuel));
    }
    drawStat("Cost", design.GetBaseCost());

    const float saveButtonH = m_layout.height * k_SaveButtonHeightRatio;
    const float saveButtonY = m_layout.y + m_layout.height - saveButtonH - padding;
    m_saveButtonRect = {m_layout.x + padding, saveButtonY, m_layout.width - padding * k_HorizontalPaddingMultiplier, saveButtonH};

    rGraphics.DrawFilledRect(
        m_saveButtonRect.x, m_saveButtonRect.y,
        m_saveButtonRect.width, m_saveButtonRect.height,
        k_SaveButtonFillColor
    );
    rGraphics.DrawRect(
        m_saveButtonRect.x, m_saveButtonRect.y,
        m_saveButtonRect.width, m_saveButtonRect.height,
        Color_t::Green()
    );
    rGraphics.DrawText(
        "Save Design",
        m_saveButtonRect.x + padding,
        m_saveButtonRect.y + saveButtonH * k_SaveButtonTextYOffsetRatio,
        statSize,
        Color_t::Green()
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
