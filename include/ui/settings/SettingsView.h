#pragma once

#include "ui/IGameView.h"

namespace ac
{

class GameSettings;

class SettingsView : public IGameView
{
public:
    SettingsView(GameSettings& rSettings, WindowLayout_t layout);

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    GameSettings& m_rSettings;
};

} // namespace ac
