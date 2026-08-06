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
        // Escape resolves rather than abandons. Simply closing would leave GetPending() set,
        // and Propose throws while it is — the council would accept no further business. Now
        // that absentees abstain (PlanetaryCouncil::Resolve), resolving is always available,
        // so there is no reason to trap the player in the view.
        PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
        if (pCouncil && pCouncil->GetPending())
        {
            pCouncil->Resolve(m_rGameState);
        }
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void CouncilVoteView::TryResolveAndClose_()
{
    PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
    if (!pCouncil || !pCouncil->GetPending())
    {
        return;
    }
    // No AllMembersVoted() gate: a member that has not voted abstains. Waiting for unanimous
    // participation is what made a silent member terminal, since nothing else clears the
    // pending slot.
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
            // Exactly the members CastElectionVote will accept. Offering full membership meant
            // the "two most populous factions" rule was decided by the UI — and now that the
            // council enforces it, offering an ineligible candidate would throw on selection.
            pCouncil->EligibleCandidates(rConfig),
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
