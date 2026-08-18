#pragma once

#include "game/council/PlanetaryCouncil.h"
#include "ui/IGameView.h"

namespace ac
{

class Faction;
class GameState;

class CouncilVoteView : public IGameView
{
public:
    CouncilVoteView(GameState& rGameState, WindowLayout_t layout);

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void OpenBallotSelector_();
    void CastElectionVote_(Faction* pCandidate);
    void CastBallot_(CouncilBallot_t ballot);
    void TryResolveAndClose_();

    GameState& m_rGameState;
};

} // namespace ac
