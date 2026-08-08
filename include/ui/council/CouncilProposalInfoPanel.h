#pragma once

#include "ui/UIElement.h"
#include "ui/council/CouncilVoteWeightCache.h"

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
    CouncilVoteWeightCache m_weights;
};

} // namespace ac
