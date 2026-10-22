#include "ui/commlinks/CommlinksView.h"
#include "ui/commlinks/CommlinksPanel.h"
#include "ui/commlinks/CouncilButton.h"
#include "ui/commlinks/CouncilCooldownPopup.h"
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

#include <stdexcept>
#include <vector>

namespace ac
{

namespace
{

// Commlinks opens only in a running game, which always has a council and a player.
const PlanetaryCouncil& RequireCouncil_(const GameState& rGameState)
{
    const PlanetaryCouncil* pCouncil = rGameState.GetPlanetaryCouncil();
    if (!pCouncil)
    {
        throw std::runtime_error("CommlinksView: no planetary council in this session");
    }
    return *pCouncil;
}

Faction& RequirePlayer_(GameState& rGameState)
{
    Faction* pPlayer = rGameState.GetPlayerFaction();
    if (!pPlayer)
    {
        throw std::runtime_error("CommlinksView: no player faction");
    }
    return *pPlayer;
}

} // namespace

CommlinksView::CommlinksView(
    GameState& rGameState,
    WindowLayout_t layout,
    std::function<void()> onOpenCouncilVote
)
    : IGameView(layout)
    , m_rGameState(rGameState)
    , m_onOpenCouncilVote(std::move(onOpenCouncilVote))
{
    const WindowLayout_t panelLayout = GetPopupLayout();
    m_elements.push_back(std::make_unique<CommlinksPanel>(m_rGameState, panelLayout));
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

void CommlinksView::OpenCouncilCooldownPopup_()
{
    const PlanetaryCouncil& rCouncil = RequireCouncil_(m_rGameState);
    const Faction& rPlayer = RequirePlayer_(m_rGameState);

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
    const PlanetaryCouncil& rCouncil = RequireCouncil_(m_rGameState);
    const Faction& rPlayer = RequirePlayer_(m_rGameState);

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
    const PlanetaryCouncil& rCouncil = RequireCouncil_(m_rGameState);
    CouncilVoteExecutor& rVotes = *m_rGameState.GetCouncilVotes();
    Faction& rPlayer = RequirePlayer_(m_rGameState);

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
