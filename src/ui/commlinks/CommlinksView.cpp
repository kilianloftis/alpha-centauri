#include "ui/commlinks/CommlinksView.h"
#include "ui/commlinks/CommlinksPanel.h"
#include "ui/commlinks/CouncilButton.h"
#include "ui/commlinks/CouncilCooldownPopup.h"
#include "ui/commlinks/CouncilProposalsPopup.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
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
    m_elements.push_back(std::make_unique<CouncilCooldownPopup>(
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        rRules.memberProposeIntervalYears,
        rRules.governorProposeIntervalYears,
        pCouncil->ProposeCooldownYears(*pPlayer),
        pCouncil->LastProposedYear(*pPlayer),
        pCouncil->YearsUntilCanPropose(m_rGameState, *pPlayer),
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
                if (pCouncil->CanPropose(*pPlayer, rProposal.id))
                {
                    available.push_back(&rProposal);
                }
            }
        }
    }

    m_elements.push_back(std::make_unique<CouncilProposalsPopup>(
        std::move(available),
        ResolveLayout(m_layout, Style().layouts.topPanel),
        [this](const CouncilProposalConfig_t& rProposal) { OnProposalSelected_(rProposal); }));
}

void CommlinksView::OnProposalSelected_(const CouncilProposalConfig_t& rProposal)
{
    PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
    Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pCouncil || !pPlayer || pCouncil->GetPending())
    {
        return;
    }
    if (pCouncil->YearsUntilCanPropose(m_rGameState, *pPlayer) > 0)
    {
        OpenCouncilCooldownPopup_();
        return;
    }
    if (!pCouncil->CanPropose(*pPlayer, rProposal.id)
        || !pCouncil->HasCommlinksToAllMembers(m_rGameState, *pPlayer))
    {
        return;
    }

    pCouncil->Propose(m_rGameState, *pPlayer, rProposal.id);
    m_bShouldClose = true;
    if (m_onOpenCouncilVote)
    {
        m_onOpenCouncilVote();
    }
}

} // namespace ac
