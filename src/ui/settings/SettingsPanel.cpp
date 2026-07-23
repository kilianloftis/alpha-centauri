#include "ui/settings/SettingsPanel.h"
#include "game/GameSettings.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <string>
#include <vector>

namespace ac
{

namespace
{

struct SettingsRow_t
{
    const char* label = nullptr;
    bool* pValue = nullptr;
    bool bHeader = false;
};

WindowLayout_t RowAt_(const WindowLayout_t& rBase, int index, float rowHeight)
{
    WindowLayout_t row = rBase;
    row.y = rBase.y + static_cast<int>(index * rowHeight * rBase.height);
    row.height = static_cast<int>(rowHeight * rBase.height);
    if (row.height < 1)
    {
        row.height = 1;
    }
    return row;
}

} // namespace

SettingsPanel::SettingsPanel(GameSettings& rSettings, WindowLayout_t layout)
    : UIElement(layout)
    , m_rSettings(rSettings)
{
}

void SettingsPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().settingsPanel;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    const WindowLayout_t titleArea = ResolveLayout(m_layout, style.titleLayout);
    const WindowLayout_t rowArea = ResolveLayout(m_layout, style.rowLayout);

    rGraphics.DrawText("Settings", titleArea.x, titleArea.y, style.titleFontSize, style.titleColor);

    GameRulesConfig_t& rRules = m_rSettings.GetGameRules();
    DebugOptionsConfig_t& rDebug = m_rSettings.GetDebugOptions();
    const std::vector<SettingsRow_t> rows = {
        {"Game Rules", nullptr, true},
        {"Pause at End of Turn", &rRules.pauseAtEndOfTurn, false},
        {"Remove Shroud", &rRules.removeShroud, false},
        {"Debug Options", nullptr, true},
        {"Remove Fog", &rDebug.removeFog, false},
    };

    constexpr float kRowHeight = 0.18f;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        const SettingsRow_t& rRow = rows[static_cast<size_t>(i)];
        const WindowLayout_t area = RowAt_(rowArea, i, kRowHeight);
        if (rRow.bHeader)
        {
            rGraphics.DrawText(rRow.label, area.x, area.y, style.rowFontSize, style.titleColor);
            continue;
        }

        const char* status = *rRow.pValue ? "On" : "Off";
        rGraphics.DrawText(
            std::string(rRow.label) + ": " + status,
            area.x,
            area.y,
            style.rowFontSize,
            style.rowColor);
    }
}

void SettingsPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const WindowLayout_t rowArea = ResolveLayout(m_layout, Style().settingsPanel.rowLayout);

    GameRulesConfig_t& rRules = m_rSettings.GetGameRules();
    DebugOptionsConfig_t& rDebug = m_rSettings.GetDebugOptions();
    const std::vector<SettingsRow_t> rows = {
        {"Game Rules", nullptr, true},
        {"Pause at End of Turn", &rRules.pauseAtEndOfTurn, false},
        {"Remove Shroud", &rRules.removeShroud, false},
        {"Debug Options", nullptr, true},
        {"Remove Fog", &rDebug.removeFog, false},
    };

    constexpr float kRowHeight = 0.18f;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        const SettingsRow_t& rRow = rows[static_cast<size_t>(i)];
        if (rRow.bHeader || !rRow.pValue)
        {
            continue;
        }
        const WindowLayout_t area = RowAt_(rowArea, i, kRowHeight);
        if (!ContainsMouseCoord(area, rEvent))
        {
            continue;
        }

        *rRow.pValue = !*rRow.pValue;
        m_rSettings.Save();
        return;
    }
}

} // namespace ac
