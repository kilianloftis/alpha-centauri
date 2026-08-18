#include "ui/council/CouncilVoteView.h"
#include "ui/ListSelectorPopup.h"

#include <array>
#include "ui/council/CouncilFactionVotesPanel.h"
#include "ui/council/CouncilProposalInfoPanel.h"
#include "ui/council/CouncilVoteButton.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/PlanetaryCouncil.h"
#include "ui/style/UiStyle.h"

#include <stdexcept>

namespace ac
{

namespace
{

// This view exists only for an active council vote. A missing council or player faction is a
// broken session, not an empty state — leaving the player with a Vote button that does nothing
// was the previous behaviour on every one of these paths.
PlanetaryCouncil& RequireCouncil_(GameState& rGameState)
{
    PlanetaryCouncil* pCouncil = rGameState.GetPlanetaryCouncil();
    if (!pCouncil)
    {
        throw std::runtime_error("CouncilVoteView: no planetary council in this session");
    }
    return *pCouncil;
}

Faction& RequirePlayer_(GameState& rGameState)
{
    Faction* pPlayer = rGameState.GetPlayerFaction();
    if (!pPlayer)
    {
        throw std::runtime_error("CouncilVoteView: no player faction");
    }
    return *pPlayer;
}

} // namespace

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
        PlanetaryCouncil& rCouncil = RequireCouncil_(m_rGameState);
        if (rCouncil.GetPending())
        {
            rCouncil.Resolve(m_rGameState);
        }
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void CouncilVoteView::TryResolveAndClose_()
{
    PlanetaryCouncil& rCouncil = RequireCouncil_(m_rGameState);
    if (!rCouncil.GetPending())
    {
        return;
    }
    // No AllMembersVoted() gate: a member that has not voted abstains. Waiting for unanimous
    // participation is what made a silent member terminal, since nothing else clears the
    // pending slot.
    rCouncil.Resolve(m_rGameState);
    m_bShouldClose = true;
}

void CouncilVoteView::OpenBallotSelector_()
{
    PlanetaryCouncil& rCouncil = RequireCouncil_(m_rGameState);
    RequirePlayer_(m_rGameState);
    if (!rCouncil.GetPending())
    {
        return;
    }

    const PlanetaryCouncil::PendingProposal_t& rPending = *rCouncil.GetPending();
    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(rPending.proposalId);
    const WindowLayout_t popupLayout = ResolveLayout(m_layout, Style().layouts.popupSmall);

    DismissOpenModals_();
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        // Exactly the members CastElectionVote will accept. Offering full membership meant the
        // "two most populous factions" rule was decided by the UI — and now that the council
        // enforces it, offering an ineligible candidate would throw on selection.
        std::vector<Faction*> candidates = rCouncil.EligibleCandidates(rConfig);
        std::vector<PopupChoice_t> choices;
        choices.reserve(candidates.size() + 1);
        for (Faction* pCandidate : candidates)
        {
            choices.push_back({pCandidate->GetDefinition().identity.name,
                               [this, pCandidate] { CastElectionVote_(pCandidate); }});
        }
        choices.push_back({"Abstain", [this] { CastElectionVote_(nullptr); }});

        m_elements.push_back(std::make_unique<ListSelectorPopup>(
            "Cast Ballot", "No candidates", std::move(choices), popupLayout,
            Style().listSelectorPopup));
        return;
    }

    std::vector<PopupChoice_t> choices;
    choices.push_back({"Yea", [this] { CastBallot_(CouncilBallot_t::Yea); }});
    choices.push_back({"Nay", [this] { CastBallot_(CouncilBallot_t::Nay); }});
    choices.push_back({"Abstain", [this] { CastBallot_(CouncilBallot_t::Abstain); }});

    m_elements.push_back(std::make_unique<ListSelectorPopup>(
        "Cast Ballot", "No ballot options", std::move(choices), popupLayout,
        Style().listSelectorPopup));
}

void CouncilVoteView::CastElectionVote_(Faction* pCandidate)
{
    RequireCouncil_(m_rGameState).CastElectionVote(RequirePlayer_(m_rGameState), pCandidate);
    TryResolveAndClose_();
}

void CouncilVoteView::CastBallot_(CouncilBallot_t ballot)
{
    RequireCouncil_(m_rGameState).CastVote(RequirePlayer_(m_rGameState), ballot);
    TryResolveAndClose_();
}

} // namespace ac
