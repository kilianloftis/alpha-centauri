#include "ui/settings/SettingsPanel.h"
#include "game/GameSettings.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <string>

namespace ac
{

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
    const WindowLayout_t rowArea   = ResolveLayout(m_layout, style.rowLayout);

    rGraphics.DrawText("Settings", titleArea.x, titleArea.y, style.titleFontSize, style.titleColor);

    const char* status = m_rSettings.IsPauseAtEndOfTurn() ? "On" : "Off";
    rGraphics.DrawText(
        std::string("Pause at End of Turn: ") + status,
        rowArea.x,
        rowArea.y,
        style.rowFontSize,
        style.rowColor);
}

void SettingsPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const WindowLayout_t rowArea = ResolveLayout(m_layout, Style().settingsPanel.rowLayout);
    if (!ContainsMouseCoord(rowArea, rEvent))
    {
        return;
    }

    m_rSettings.SetPauseAtEndOfTurn(!m_rSettings.IsPauseAtEndOfTurn());
    m_rSettings.Save();
}

} // namespace ac
