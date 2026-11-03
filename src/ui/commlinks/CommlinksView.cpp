#include "ui/commlinks/CommlinksView.h"
#include "ui/commlinks/CommlinksPanel.h"
#include "ui/commlinks/CouncilButton.h"
#include "ui/commlinks/CouncilCooldownPopup.h"
#include "ui/commlinks/TalkPopup.h"
#include "ui/ListSelectorPopup.h"
#include "ui/NoticePopup.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRules.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/council/PlanetaryCouncil.h"
#include "ui/style/UiStyle.h"

#include <random>
#include <stdexcept>
#include <vector>

namespace ac
{

CommlinksView::CommlinksView(
    GameState& rGameState,
    const StandingCaptions& rStandingCaptions,
    const TextPack& rText,
    const TalkScript& rTalkScript,
    WindowLayout_t layout,
    std::function<void()> onOpenCouncilVote
)
    : IGameView(layout)
    , m_rGameState(rGameState)
    , m_rStandingCaptions(rStandingCaptions)
    , m_rText(rText)
    , m_rTalkScript(rTalkScript)
    , m_onOpenCouncilVote(std::move(onOpenCouncilVote))
{
    const WindowLayout_t panelLayout = GetPopupLayout();
    m_elements.push_back(std::make_unique<CommlinksPanel>(
        m_rGameState, rStandingCaptions, panelLayout,
        [this](FactionId_t rivalId) { OpenTalk_(rivalId); }));
    const WindowLayout_t buttonLayout =
        ResolveLayout(panelLayout, Style().commlinksPanel.councilButtonLayout);
    m_elements.push_back(std::make_unique<CouncilButton>(
        buttonLayout, [this]() { OpenCouncilProposals_(); }));
}

bool CommlinksView::HandleKey(const KeyEvent_t& rEvent)
{
    if (IGameView::HandleKey(rEvent))
    {
        return true;
    }
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void CommlinksView::OpenTalk_(FactionId_t rivalId)
{
    DismissOpenModals_();
    m_elements.push_back(std::make_unique<TalkPopup>(m_rGameState, m_rTalkScript, m_rText,
                                                     m_rStandingCaptions, rivalId,
                                                     std::random_device{}(), GetPopupLayout()));
}

void CommlinksView::OpenCouncilCooldownPopup_()
{
    const PlanetaryCouncil& rCouncil = m_rGameState.GetPlanetaryCouncil();
    const Faction& rPlayer = m_rGameState.GetPlayerFaction();

    const CouncilRulesConfig_t& rRules = rCouncil.GetRules();
    // OK dismisses this council cooldown UI (and the parent Commlinks overlay).
    DismissOpenModals_();
    auto pPopup = std::make_unique<CouncilCooldownPopup>(
        GetPopupLayout(),
        rRules.memberProposeIntervalYears,
        rRules.governorProposeIntervalYears,
        ProposeCooldownYears(rCouncil, rPlayer),
        rCouncil.LastProposedYear(rPlayer),
        YearsUntilCanPropose(rCouncil, rPlayer),
        [this]() { m_bShouldClose = true; });
    m_elements.push_back(std::move(pPopup));
}

void CommlinksView::OpenCouncilProposals_()
{
    const PlanetaryCouncil& rCouncil = m_rGameState.GetPlanetaryCouncil();
    const Faction& rPlayer = m_rGameState.GetPlayerFaction();

    std::vector<PopupChoice_t> choices;
    for (const CouncilProposalConfig_t& rProposal : rCouncil.GetRegistry().GetAll())
    {
        if (CanPropose(rCouncil, rPlayer, rProposal.id))
        {
            const CouncilProposalConfig_t* pProposal = &rProposal;
            choices.push_back({pProposal->name,
                               [this, pProposal] { OnProposalSelected_(*pProposal); }});
        }
    }

    DismissOpenModals_();
    auto pPopup = std::make_unique<ListSelectorPopup>(
        "Council Proposals", "No proposals available", std::move(choices),
        ResolveLayout(m_layout, Style().layouts.topPanel), Style().listSelectorPopup, nullptr);
    m_elements.push_back(std::move(pPopup));
}

void CommlinksView::ShowNotice_(std::string message)
{
    DismissOpenModals_();
    auto pPopup = std::make_unique<NoticePopup>(
        GetPopupLayout(), "Planetary Council", std::move(message), nullptr, nullptr);
    m_elements.push_back(std::move(pPopup));
}

void CommlinksView::OnProposalSelected_(const CouncilProposalConfig_t& rProposal)
{
    const PlanetaryCouncil& rCouncil = m_rGameState.GetPlanetaryCouncil();
    CouncilVoteExecutor& rVotes = m_rGameState.GetCouncilVotes();
    Faction& rPlayer = m_rGameState.GetPlayerFaction();

    // The proposals popup closes itself on selection, so each gate below explains itself.
    if (rVotes.GetPending())
    {
        ShowNotice_("The council is already considering a proposal.");
        return;
    }
    if (YearsUntilCanPropose(rCouncil, rPlayer) > 0)
    {
        OpenCouncilCooldownPopup_();
        return;
    }
    if (!HasCommlinksToAllMembers(rCouncil, rPlayer))
    {
        ShowNotice_("You must have commlinks with every council member to propose.");
        return;
    }
    if (!CanPropose(rCouncil, rPlayer, rProposal.id))
    {
        ShowNotice_("You cannot propose " + rProposal.name + " right now.");
        return;
    }

    rVotes.Propose(m_rGameState, rPlayer, rProposal.id);
    m_bShouldClose = true;
    m_onOpenCouncilVote();
}

} // namespace ac
