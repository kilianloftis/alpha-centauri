#pragma once

#include "ui/UIElement.h"
#include "ui/council/CouncilVoteWeightCache.h"

#include <string>

namespace ac
{

class GameState;
class PlanetaryCouncil;
struct CouncilProposalConfig_t;
struct PendingProposal_t;

// Center-panel summary: "Proposal", name, option tallies (Vote button is a sibling element).
class CouncilProposalInfoPanel : public UIElement
{
public:
    CouncilProposalInfoPanel(GameState& rGameState, WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    std::string ElectionTally_(const PlanetaryCouncil& rCouncil,
                               const PendingProposal_t& rPending,
                               const CouncilProposalConfig_t& rConfig);
    std::string BallotTally_(const PlanetaryCouncil& rCouncil,
                             const PendingProposal_t& rPending,
                             const CouncilProposalConfig_t& rConfig);

    GameState& m_rGameState;
    CouncilVoteWeightCache m_weights;
};

} // namespace ac
