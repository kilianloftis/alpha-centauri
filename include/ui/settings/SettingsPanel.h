#pragma once

#include "ui/UIElement.h"

namespace ac
{

class GameSettings;

class SettingsPanel : public UIElement
{
public:
    SettingsPanel(GameSettings& rSettings, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    GameSettings& m_rSettings;
};

} // namespace ac
