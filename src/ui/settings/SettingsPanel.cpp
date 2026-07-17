#include "ui/settings/SettingsPanel.h"
#include "game/GameSettings.h"
#include "graphics/Graphics.h"

#include <string>

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor {30, 30, 50, 255};
constexpr Color_t k_BorderColor     {80, 80, 120, 255};
constexpr unsigned int k_TitleFontSize = 18;
constexpr unsigned int k_RowFontSize   = 16;
constexpr RatioLayout_t k_TitleLayout {0.05f, 0.05f, 0.9f, 0.15f};
constexpr RatioLayout_t k_RowLayout   {0.05f, 0.25f, 0.9f, 0.2f};

} // namespace

SettingsPanel::SettingsPanel(GameSettings& rSettings, WindowLayout_t layout)
    : UIElement(layout)
    , m_rSettings(rSettings)
{
}

void SettingsPanel::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);

    const WindowLayout_t titleArea = ResolveLayout(m_layout, k_TitleLayout);
    const WindowLayout_t rowArea   = ResolveLayout(m_layout, k_RowLayout);

    rGraphics.DrawText("Settings", titleArea.x, titleArea.y, k_TitleFontSize, Color_t::White());

    const char* status = m_rSettings.IsPauseAtEndOfTurn() ? "On" : "Off";
    rGraphics.DrawText(
        std::string("Pause at End of Turn: ") + status,
        rowArea.x,
        rowArea.y,
        k_RowFontSize,
        Color_t::Yellow());
}

void SettingsPanel::HandleMouseClick(const MouseEvent_t& rEvent)
{
    const WindowLayout_t rowArea = ResolveLayout(m_layout, k_RowLayout);
    if (!ContainsMouseCoord(rowArea, rEvent))
    {
        return;
    }

    m_rSettings.SetPauseAtEndOfTurn(!m_rSettings.IsPauseAtEndOfTurn());
    m_rSettings.Save();
}

} // namespace ac
