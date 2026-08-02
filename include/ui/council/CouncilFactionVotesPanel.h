#pragma once

#include "ui/UIElement.h"

namespace ac
{

class GameState;

// Top-panel columns: one vertical slice per council member (ballot + vote weight).
class CouncilFactionVotesPanel : public UIElement
{
public:
    CouncilFactionVotesPanel(GameState& rGameState, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    GameState& m_rGameState;
};

} // namespace ac
