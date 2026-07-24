#include "ui/settings/SettingsPanel.h"
#include "game/GameSettings.h"
#include "graphics/Graphics.h"
#include "ui/settings/SettingDescriptor.h"
#include "ui/style/UiStyle.h"

#include <cstddef>
#include <cstdio>
#include <string>

namespace ac
{

namespace
{

bool GetPauseAtEndOfTurn_(const GameSettings& rSettings)
{
    return rSettings.GetGameRules().pauseAtEndOfTurn;
}

void SetPauseAtEndOfTurn_(GameSettings& rSettings, bool value)
{
    rSettings.SetPauseAtEndOfTurn(value);
}

bool GetRemoveShroud_(const GameSettings& rSettings)
{
    return rSettings.GetVisibility().removeShroud;
}

void SetRemoveShroud_(GameSettings& rSettings, bool value)
{
    VisibilityConfig_t visibility = rSettings.GetVisibility();
    visibility.removeShroud = value;
    rSettings.SetVisibility(visibility);
}

bool GetRemoveFog_(const GameSettings& rSettings)
{
    return rSettings.GetVisibility().removeFog;
}

void SetRemoveFog_(GameSettings& rSettings, bool value)
{
    VisibilityConfig_t visibility = rSettings.GetVisibility();
    visibility.removeFog = value;
    rSettings.SetVisibility(visibility);
}

std::string GetMapWidthText_(const GameSettings& rSettings)
{
    return std::to_string(rSettings.GetMapGeneration().width);
}

std::string GetMapHeightText_(const GameSettings& rSettings)
{
    return std::to_string(rSettings.GetMapGeneration().height);
}

std::string GetOceanCoverageText_(const GameSettings& rSettings)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", rSettings.GetMapGeneration().oceanCoverage);
    return buffer;
}

std::string GetPresetIdText_(const GameSettings& rSettings)
{
    return rSettings.GetMapGeneration().presetId;
}

std::string GetSeedText_(const GameSettings& rSettings)
{
    const unsigned int seed = rSettings.GetMapGeneration().seed;
    if (seed == 0)
    {
        return "random";
    }
    return std::to_string(seed);
}

// Static descriptor table: no bool* bindings into live settings members.
const SettingDescriptor_t k_SettingDescriptors[] = {
    {"Game Rules", SettingRowKind_t::Header},
    {"Pause at End of Turn", SettingRowKind_t::Bool, SettingScope_t::Always,
     GetPauseAtEndOfTurn_, SetPauseAtEndOfTurn_},
    {"Visibility", SettingRowKind_t::Header},
    {"Remove Shroud", SettingRowKind_t::Bool, SettingScope_t::Always,
     GetRemoveShroud_, SetRemoveShroud_},
    {"Remove Fog", SettingRowKind_t::Bool, SettingScope_t::Always,
     GetRemoveFog_, SetRemoveFog_},
    {"Map Generation", SettingRowKind_t::Header, SettingScope_t::NewGameOnly},
    {"Width", SettingRowKind_t::ReadOnlyValue, SettingScope_t::NewGameOnly,
     nullptr, nullptr, GetMapWidthText_},
    {"Height", SettingRowKind_t::ReadOnlyValue, SettingScope_t::NewGameOnly,
     nullptr, nullptr, GetMapHeightText_},
    {"Ocean Coverage", SettingRowKind_t::ReadOnlyValue, SettingScope_t::NewGameOnly,
     nullptr, nullptr, GetOceanCoverageText_},
    {"Preset", SettingRowKind_t::ReadOnlyValue, SettingScope_t::NewGameOnly,
     nullptr, nullptr, GetPresetIdText_},
    {"Seed", SettingRowKind_t::ReadOnlyValue, SettingScope_t::NewGameOnly,
     nullptr, nullptr, GetSeedText_},
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

    constexpr int kRowCount = static_cast<int>(std::size(k_SettingDescriptors));
    const float rowHeight = 1.0f / static_cast<float>(kRowCount);
    for (int i = 0; i < kRowCount; ++i)
    {
        const SettingDescriptor_t& rRow = k_SettingDescriptors[static_cast<size_t>(i)];
        const WindowLayout_t area = RowAt_(rowArea, i, rowHeight);
        if (rRow.kind == SettingRowKind_t::Header)
        {
            rGraphics.DrawText(rRow.label, area.x, area.y, style.rowFontSize, style.titleColor);
            continue;
        }

        std::string text;
        if (rRow.kind == SettingRowKind_t::Bool)
        {
            const char* status = rRow.getBool(m_rSettings) ? "On" : "Off";
            text = std::string(rRow.label) + ": " + status;
        }
        else
        {
            text = std::string(rRow.label) + ": " + rRow.getValueText(m_rSettings);
            if (rRow.scope == SettingScope_t::NewGameOnly)
            {
                text += " (new game)";
            }
        }

        rGraphics.DrawText(text, area.x, area.y, style.rowFontSize, style.rowColor);
    }
}

void SettingsPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const WindowLayout_t rowArea = ResolveLayout(m_layout, Style().settingsPanel.rowLayout);

    constexpr int kRowCount = static_cast<int>(std::size(k_SettingDescriptors));
    const float rowHeight = 1.0f / static_cast<float>(kRowCount);
    for (int i = 0; i < kRowCount; ++i)
    {
        const SettingDescriptor_t& rRow = k_SettingDescriptors[static_cast<size_t>(i)];
        if (rRow.kind != SettingRowKind_t::Bool || rRow.scope == SettingScope_t::NewGameOnly)
        {
            continue;
        }
        const WindowLayout_t area = RowAt_(rowArea, i, rowHeight);
        if (!ContainsMouseCoord(area, rEvent))
        {
            continue;
        }

        rRow.setBool(m_rSettings, !rRow.getBool(m_rSettings));
        m_rSettings.Save();
        return;
    }
}

} // namespace ac
