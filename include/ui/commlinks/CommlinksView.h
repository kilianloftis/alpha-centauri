#pragma once

#include "game/base/BaseTypes.h"
#include "ui/IGameView.h"

#include <string>
#include <functional>

namespace ac
{

class GameState;
class StandingCaptions;
class TalkScript;
class TextPack;
struct CouncilProposalConfig_t;

class CommlinksView : public IGameView
{
public:
    CommlinksView(
        GameState& rGameState,
        const StandingCaptions& rStandingCaptions,
        const TextPack& rText,
        const TalkScript& rTalkScript,
        WindowLayout_t layout,
        std::function<void()> onOpenCouncilVote
    );

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void OpenTalk_(FactionId_t rivalId);
    void OpenCouncilProposals_();
    void OpenCouncilCooldownPopup_();
    void OnProposalSelected_(const CouncilProposalConfig_t& rProposal);
    void ShowNotice_(std::string message);

    GameState& m_rGameState;
    const StandingCaptions& m_rStandingCaptions;
    const TextPack& m_rText;
    const TalkScript& m_rTalkScript;
    std::function<void()> m_onOpenCouncilVote;
};

} // namespace ac
