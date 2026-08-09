#include "ui/settings/SettingsPanel.h"
#include "game/GameSettings.h"
#include "graphics/Graphics.h"
#include "ui/settings/SettingDescriptor.h"
#include "ui/style/UiStyle.h"

#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

bool GetPauseAtEndOfTurn_(const GameSettings& rSettings)
{
    return rSettings.IsPauseAtEndOfTurn();
}

void SetPauseAtEndOfTurn_(GameSettings& rSettings, bool value)
{
    rSettings.SetPauseAtEndOfTurn(value);
}

bool GetAutoReturnLowFuelAir_(const GameSettings& rSettings)
{
    return rSettings.IsAutoReturnLowFuelAir();
}

void SetAutoReturnLowFuelAir_(GameSettings& rSettings, bool value)
{
    rSettings.SetAutoReturnLowFuelAir(value);
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

std::string GetErosiveForcesText_(const GameSettings& rSettings)
{
    return ToString(rSettings.GetMapGeneration().erosiveForces);
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
    {"Auto-Return Low-Fuel Aircraft", SettingRowKind_t::Bool, SettingScope_t::Always,
     GetAutoReturnLowFuelAir_, SetAutoReturnLowFuelAir_},
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
    {"Erosive Forces", SettingRowKind_t::ReadOnlyValue, SettingScope_t::NewGameOnly,
     nullptr, nullptr, GetErosiveForcesText_},
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

// Walk the table once, handing each row its on-screen area. Render and HandleMouseClick used to
// each recompute the row count, the row height and the area, so a change to one could silently
// paint rows where the other did not click them.
template <typename Visitor>
void ForEachRow_(const WindowLayout_t& rRowArea, Visitor&& visit)
{
    constexpr int kRowCount = static_cast<int>(std::size(k_SettingDescriptors));
    const float rowHeight = 1.0f / static_cast<float>(kRowCount);
    for (int i = 0; i < kRowCount; ++i)
    {
        visit(k_SettingDescriptors[static_cast<size_t>(i)], RowAt_(rRowArea, i, rowHeight));
    }
}

// A descriptor row must carry the callbacks its kind uses. A mismatched table entry is a
// programming error in this file, and calling a null function pointer is undefined behaviour.
void RequireCallbacks_(const SettingDescriptor_t& rRow)
{
    const std::string label = rRow.label ? rRow.label : "<unlabelled>";
    switch (rRow.kind)
    {
    case SettingRowKind_t::Header:
        return;
    case SettingRowKind_t::Bool:
        if (!rRow.getBool || !rRow.setBool)
        {
            throw std::runtime_error("Setting row '" + label + "' is Bool but has no getBool/setBool");
        }
        // Nothing can edit a NewGameOnly bool: the panel has no new-game/in-progress flag, so
        // HandleMouseClick skips it forever. Until that flag exists, such a row is a table error
        // rather than a disabled control.
        if (rRow.scope == SettingScope_t::NewGameOnly)
        {
            throw std::runtime_error("Setting row '" + label
                                     + "' is a NewGameOnly Bool, which nothing can ever toggle");
        }
        return;
    case SettingRowKind_t::ReadOnlyValue:
        if (!rRow.getValueText)
        {
            throw std::runtime_error("Setting row '" + label
                                     + "' is ReadOnlyValue but has no getValueText");
        }
        return;
    }
    throw std::runtime_error("Setting row '" + label + "' has an unhandled kind");
}

} // namespace

SettingsPanel::SettingsPanel(GameSettings& rSettings, WindowLayout_t layout)
    : UIElement(layout)
    , m_rSettings(rSettings)
{
}

void SettingsPanel::Render(Graphics& rGraphics)
{
    const SettingsPanelStyle_t& rStyle = Style().settingsPanel;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, rStyle.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, rStyle.borderColor);

    const WindowLayout_t titleArea = ResolveLayout(m_layout, rStyle.titleLayout);
    const WindowLayout_t rowArea = ResolveLayout(m_layout, rStyle.rowLayout);

    rGraphics.DrawText("Settings", titleArea.x, titleArea.y, rStyle.titleFontSize, rStyle.titleColor);

    ForEachRow_(rowArea, [&](const SettingDescriptor_t& rRow, const WindowLayout_t& rArea)
    {
        RequireCallbacks_(rRow);

        // Exhaustive: a new SettingRowKind_t must be handled here rather than falling into the
        // read-only branch and calling whatever function pointer happens to be null.
        switch (rRow.kind)
        {
        case SettingRowKind_t::Header:
            rGraphics.DrawText(rRow.label, rArea.x, rArea.y, rStyle.rowFontSize,
                               rStyle.titleColor);
            return;
        case SettingRowKind_t::Bool:
        {
            const char* pStatus = rRow.getBool(m_rSettings) ? "On" : "Off";
            rGraphics.DrawText(std::string(rRow.label) + ": " + pStatus, rArea.x, rArea.y,
                               rStyle.rowFontSize, rStyle.rowColor);
            return;
        }
        case SettingRowKind_t::ReadOnlyValue:
        {
            std::string text = std::string(rRow.label) + ": " + rRow.getValueText(m_rSettings);
            if (rRow.scope == SettingScope_t::NewGameOnly)
            {
                text += " (new game)";
            }
            rGraphics.DrawText(text, rArea.x, rArea.y, rStyle.rowFontSize, rStyle.rowColor);
            return;
        }
        }
        throw std::runtime_error("SettingsPanel: unhandled setting row kind");
    });
}

void SettingsPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    // Every peer element requires the left button. This one acted on any button that reached
    // it, so a right-click flipped a preference and wrote user_settings.json.
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    const WindowLayout_t rowArea = ResolveLayout(m_layout, Style().settingsPanel.rowLayout);

    bool bHandled = false;
    ForEachRow_(rowArea, [&](const SettingDescriptor_t& rRow, const WindowLayout_t& rArea)
    {
        if (bHandled || rRow.kind != SettingRowKind_t::Bool
            || !ContainsMouseCoord(rArea, rEvent))
        {
            return;
        }
        RequireCallbacks_(rRow);
        rRow.setBool(m_rSettings, !rRow.getBool(m_rSettings));
        m_rSettings.Save();
        bHandled = true;
    });
}

} // namespace ac
