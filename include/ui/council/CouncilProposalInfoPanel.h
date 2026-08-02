#pragma once

#include "ui/UIElement.h"

namespace ac
{

class GameState;

// Center-panel summary: "Proposal", name, option tallies (Vote button is a sibling element).
class CouncilProposalInfoPanel : public UIElement
{
public:
    CouncilProposalInfoPanel(GameState& rGameState, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    GameState& m_rGameState;
};

} // namespace ac
