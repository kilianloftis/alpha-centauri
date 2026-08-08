#pragma once

#include "ui/IGameView.h"

#include <string>
#include <functional>

namespace ac
{

class GameState;
struct CouncilProposalConfig_t;

class CommlinksView : public IGameView
{
public:
    CommlinksView(
        GameState& rGameState,
        WindowLayout_t layout,
        std::function<void()> onOpenCouncilVote
    );

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void OpenCouncilProposals_();
    void OpenCouncilCooldownPopup_();
    void OnProposalSelected_(const CouncilProposalConfig_t& rProposal);
    void ShowNotice_(std::string message);

    GameState& m_rGameState;
    std::function<void()> m_onOpenCouncilVote;
};

} // namespace ac
