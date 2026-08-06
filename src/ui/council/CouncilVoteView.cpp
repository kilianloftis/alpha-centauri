#include "ui/council/CouncilVoteView.h"
#include "ui/council/CouncilBallotPopup.h"
#include "ui/council/CouncilFactionVotesPanel.h"
#include "ui/council/CouncilProposalInfoPanel.h"
#include "ui/council/CouncilVoteButton.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/PlanetaryCouncil.h"
#include "ui/style/UiStyle.h"

namespace ac
{

CouncilVoteView::CouncilVoteView(GameState& rGameState, WindowLayout_t layout)
    : IGameView(layout)
    , m_rGameState(rGameState)
{
    const auto& style = Style().councilVoteView;
    const WindowLayout_t topPanel = ResolveLayout(m_layout, Style().layouts.topPanel);
    const WindowLayout_t centerPanel = ResolveLayout(m_layout, Style().layouts.centerPanel);

    m_elements.push_back(std::make_unique<CouncilFactionVotesPanel>(m_rGameState, topPanel));
    m_elements.push_back(std::make_unique<CouncilProposalInfoPanel>(m_rGameState, centerPanel));
    m_elements.push_back(std::make_unique<CouncilVoteButton>(
        ResolveLayout(centerPanel, style.voteButtonLayout),
        [this]() { OpenBallotSelector_(); }));
}

bool CouncilVoteView::HandleKey(const KeyEvent_t& rEvent)
{
    if (IGameView::HandleKey(rEvent))
    {
        return true;
    }
    if (rEvent.key == Key_t::Escape)
    {
        // Refuse dismiss while a ballot is still open — closing would leave GetPending() set
        // and brick further proposals (Package 5 owns absentee/resolve exits).
        const PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
        if (pCouncil && pCouncil->GetPending())
        {
            return true;
        }
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void CouncilVoteView::TryResolveAndClose_()
{
    PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
    if (!pCouncil || !pCouncil->AllMembersVoted())
    {
        return;
    }
    pCouncil->Resolve(m_rGameState);
    m_bShouldClose = true;
}

void CouncilVoteView::OpenBallotSelector_()
{
    PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
    Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pCouncil || !pPlayer || !pCouncil->GetPending())
    {
        return;
    }

    const PlanetaryCouncil::PendingProposal_t& rPending = *pCouncil->GetPending();
    const CouncilProposalConfig_t& rConfig = pCouncil->GetRegistry().Get(rPending.proposalId);
    const WindowLayout_t popupLayout = ResolveLayout(m_layout, Style().layouts.popupSmall);

    DismissOpenModals_();
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        m_elements.push_back(CouncilBallotPopup::CreateElection(
            popupLayout,
            pCouncil->Members(),
            [this](const Faction* pCandidate) {
                PlanetaryCouncil* pCouncilInner = m_rGameState.GetPlanetaryCouncil();
                Faction* pPlayerInner = m_rGameState.GetPlayerFaction();
                if (pCouncilInner && pPlayerInner)
                {
                    pCouncilInner->CastElectionVote(*pPlayerInner, pCandidate);
                    TryResolveAndClose_();
                }
            }));
        return;
    }

    m_elements.push_back(CouncilBallotPopup::CreateStandard(
        popupLayout,
        [this](CouncilBallot_t ballot) {
            PlanetaryCouncil* pCouncilInner = m_rGameState.GetPlanetaryCouncil();
            Faction* pPlayerInner = m_rGameState.GetPlayerFaction();
            if (pCouncilInner && pPlayerInner)
            {
                pCouncilInner->CastVote(*pPlayerInner, ballot);
                TryResolveAndClose_();
            }
        }));
}

} // namespace ac
