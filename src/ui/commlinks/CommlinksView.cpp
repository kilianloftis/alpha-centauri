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

#include <vector>

namespace ac
{

CommlinksView::CommlinksView(
    GameState& rGameState,
    WindowLayout_t layout,
    std::function<void()> onOpenCouncilVote
)
    : IGameView(layout)
    , m_rGameState(rGameState)
    , m_onOpenCouncilVote(std::move(onOpenCouncilVote))
{
    const WindowLayout_t panelLayout = ResolveLayout(m_layout, Style().layouts.popupSmall);
    m_elements.push_back(std::make_unique<CommlinksPanel>(m_rGameState, panelLayout));
    m_elements.push_back(std::make_unique<CouncilButton>(
        ResolveLayout(panelLayout, Style().commlinksPanel.councilButtonLayout),
        [this]() { OpenCouncilProposals_(); }));
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
    const PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pCouncil || !pPlayer)
    {
        return;
    }

    const CouncilRulesConfig_t& rRules = pCouncil->GetRules();
    // OK dismisses this council cooldown UI (and the parent Commlinks overlay).
    DismissOpenModals_();
    m_elements.push_back(std::make_unique<CouncilCooldownPopup>(
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        rRules.memberProposeIntervalYears,
        rRules.governorProposeIntervalYears,
        ProposeCooldownYears(*pCouncil, *pPlayer),
        pCouncil->LastProposedYear(*pPlayer),
        YearsUntilCanPropose(*pCouncil, *pPlayer),
        [this]() { m_bShouldClose = true; }));
}

void CommlinksView::OpenCouncilProposals_()
{
    std::vector<const CouncilProposalConfig_t*> available;
    if (const PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil())
    {
        if (const Faction* pPlayer = m_rGameState.GetPlayerFaction())
        {
            for (const CouncilProposalConfig_t& rProposal : pCouncil->GetRegistry().GetAll())
            {
                if (CanPropose(*pCouncil, *pPlayer, rProposal.id))
                {
                    available.push_back(&rProposal);
                }
            }
        }
    }

    std::vector<PopupChoice_t> choices;
    choices.reserve(available.size());
    for (const CouncilProposalConfig_t* pProposal : available)
    {
        choices.push_back({pProposal->name,
                           [this, pProposal] { OnProposalSelected_(*pProposal); }});
    }

    DismissOpenModals_();
    m_elements.push_back(std::make_unique<ListSelectorPopup>(
        "Council Proposals", "No proposals available", std::move(choices),
        ResolveLayout(m_layout, Style().layouts.topPanel), Style().listSelectorPopup));
}

void CommlinksView::ShowNotice_(std::string message)
{
    DismissOpenModals_();
    m_elements.push_back(std::make_unique<NoticePopup>(
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        "Planetary Council",
        std::move(message)));
}

void CommlinksView::OnProposalSelected_(const CouncilProposalConfig_t& rProposal)
{
    const PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
    CouncilVoteExecutor* pVotes = m_rGameState.GetCouncilVotes();
    Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pCouncil || !pPlayer)
    {
        return;
    }

    // The proposals popup closes itself on selection, so each gate below explains itself.
    if (pVotes->GetPending())
    {
        ShowNotice_("The council is already considering a proposal.");
        return;
    }
    if (YearsUntilCanPropose(*pCouncil, *pPlayer) > 0)
    {
        OpenCouncilCooldownPopup_();
        return;
    }
    if (!HasCommlinksToAllMembers(*pCouncil, *pPlayer))
    {
        ShowNotice_("You must have commlinks with every council member to propose.");
        return;
    }
    if (!CanPropose(*pCouncil, *pPlayer, rProposal.id))
    {
        ShowNotice_("You cannot propose " + rProposal.name + " right now.");
        return;
    }

    pVotes->Propose(m_rGameState, *pPlayer, rProposal.id);
    m_bShouldClose = true;
    if (m_onOpenCouncilVote)
    {
        m_onOpenCouncilVote();
    }
}

} // namespace ac
