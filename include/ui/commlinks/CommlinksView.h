#pragma once

#include "ui/IGameView.h"

namespace ac
{

class GameState;

class CommlinksView : public IGameView
{
public:
    CommlinksView(GameState& rGameState, WindowLayout_t layout);

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    GameState& m_rGameState;
};

} // namespace ac
