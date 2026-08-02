#pragma once

#include "ui/IGameView.h"

namespace ac
{

class GameState;

class CouncilVoteView : public IGameView
{
public:
    CouncilVoteView(GameState& rGameState, WindowLayout_t layout);

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void OpenBallotSelector_();
    void TryResolveAndClose_();

    GameState& m_rGameState;
};

} // namespace ac
