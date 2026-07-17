#include "ui/settings/SettingsView.h"
#include "ui/settings/SettingsPanel.h"
#include "game/GameSettings.h"

namespace ac
{

SettingsView::SettingsView(GameSettings& rSettings, WindowLayout_t layout)
    : IGameView(layout)
    , m_rSettings(rSettings)
{
    m_elements.push_back(std::make_unique<SettingsPanel>(
        m_rSettings,
        ResolveLayout(m_layout, k_PopupLayoutSmall)));
}

bool SettingsView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

} // namespace ac
