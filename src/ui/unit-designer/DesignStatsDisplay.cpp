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

struct StatsMetrics_t
{
    float padding;
    unsigned int headerSize;
    unsigned int statSize;
    float lineH;
};

StatsMetrics_t MetricsFor_(const WindowLayout_t& rLayout)
{
    const auto& style = Style().designStatsDisplay;
    return {
        rLayout.height * style.paddingRatio,
        static_cast<unsigned int>(rLayout.height * style.headerFontSizeRatio),
        static_cast<unsigned int>(rLayout.height * style.statFontSizeRatio),
        rLayout.height * style.lineHeightRatio
    };
}

void DrawStats_(Graphics& rGraphics,
                const UnitDesign& rDesign,
                const WindowLayout_t& rLayout,
                const StatsMetrics_t& rMetrics)
{
    float y = rLayout.y + rMetrics.padding + rMetrics.lineH;
    auto drawStat = [&](const std::string& rLabel, int value)
    {
        std::ostringstream oss;
        oss << rLabel << ": " << value;
        rGraphics.DrawText(oss.str(), rLayout.x + rMetrics.padding, y, rMetrics.statSize,
                           Style().designStatsDisplay.statTextColor);
        y += rMetrics.lineH;
    };

    drawStat("Attack",   ResolveStat(rDesign, StatId_t::Attack));
    drawStat("Defense",  ResolveStat(rDesign, StatId_t::Defense));
    drawStat("Movement", rDesign.GetMovementPoints());
    drawStat("HP",       ResolveStat(rDesign, StatId_t::HitPoints));
    if (rDesign.UsesFuel())
    {
        drawStat("Fuel", rDesign.MaxFuel());
    }
    drawStat("Cost", rDesign.GetBaseCost());
}

// Returns the button's rect, which is also its click target.
Rectangle_t DrawSaveButton_(Graphics& rGraphics,
                            const WindowLayout_t& rLayout,
                            const StatsMetrics_t& rMetrics)
{
    const auto& style = Style().designStatsDisplay;
    const float padding = rMetrics.padding;
    const float buttonH = rLayout.height * style.saveButtonHeightRatio;
    const Rectangle_t rect{
        rLayout.x + padding,
        rLayout.y + rLayout.height - buttonH - padding,
        rLayout.width - padding * style.horizontalPaddingMultiplier,
        buttonH
    };

    rGraphics.DrawFilledRect(rect.x, rect.y, rect.width, rect.height, style.saveButtonFillColor);
    rGraphics.DrawRect(rect.x, rect.y, rect.width, rect.height, style.saveButtonBorderColor);
    const float textY = rect.y + buttonH * style.saveButtonTextYOffsetRatio;
    rGraphics.DrawText("Save Design", rect.x + padding, textY, rMetrics.statSize,
                       style.saveButtonTextColor);
    return rect;
}

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
    const auto& style = Style().designStatsDisplay;
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    const StatsMetrics_t metrics = MetricsFor_(m_layout);
    const float textX = m_layout.x + metrics.padding;
    const float headerY = m_layout.y + metrics.padding;
    rGraphics.DrawText("Design Stats", textX, headerY, metrics.headerSize, style.headerColor);

    if (!m_pState->HasAllMandatory(*m_pSlots))
    {
        rGraphics.DrawText("Fill all required slots", textX, headerY + metrics.lineH,
                           metrics.statSize, style.incompleteTextColor);
        return;
    }

    const UnitDesign design(*m_pSlots, m_pState->components);
    DrawStats_(rGraphics, design, m_layout, metrics);
    m_saveButtonRect = DrawSaveButton_(rGraphics, m_layout, metrics);
}

void DesignStatsDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left || !m_pState->HasAllMandatory(*m_pSlots))
    {
        return;
    }

    if (ContainsMouseCoord(m_saveButtonRect, rEvent))
    {
        m_onSaveDesign();
    }
}

} // namespace ac
