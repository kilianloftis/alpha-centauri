#include "ui/council/CouncilVoteView.h"
#include "ui/ListSelectorPopup.h"

#include <array>
#include "ui/council/CouncilFactionVotesPanel.h"
#include "ui/council/CouncilProposalInfoPanel.h"
#include "ui/council/CouncilVoteButton.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRules.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/council/PlanetaryCouncil.h"
#include "ui/style/UiStyle.h"

#include <stdexcept>

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
        // Escape resolves the vote: closing would leave GetPending() set, and Propose throws
        // while it is. Absentees abstain, so resolving is always available.
        CouncilVoteExecutor& rVotes = m_rGameState.GetCouncilVotes();
        if (rVotes.GetPending())
        {
            rVotes.Resolve(m_rGameState);
        }
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void CouncilVoteView::TryResolveAndClose_()
{
    CouncilVoteExecutor& rVotes = m_rGameState.GetCouncilVotes();
    if (!rVotes.GetPending())
    {
        return;
    }
    // No AllMembersVoted() gate: a member that has not voted abstains, and nothing else clears
    // the pending slot.
    rVotes.Resolve(m_rGameState);
    m_bShouldClose = true;
}

void CouncilVoteView::OpenBallotSelector_()
{
    const PlanetaryCouncil& rCouncil = m_rGameState.GetPlanetaryCouncil();
    const CouncilVoteExecutor& rVotes = m_rGameState.GetCouncilVotes();
    if (!rVotes.GetPending())
    {
        return;
    }

    const PendingProposal_t& rPending = *rVotes.GetPending();
    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(rPending.proposalId);
    const WindowLayout_t popupLayout = GetPopupLayout();

    DismissOpenModals_();
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        // Exactly the members CastElectionVote will accept; an ineligible one would throw.
        std::vector<Faction*> candidates = EligibleCandidates(rCouncil, rConfig);
        std::vector<PopupChoice_t> choices;
        choices.reserve(candidates.size() + 1);
        for (Faction* pCandidate : candidates)
        {
            choices.push_back({pCandidate->GetDefinition().identity.name,
                               [this, pCandidate] { CastElectionVote_(pCandidate); }});
        }
        choices.push_back({"Abstain", [this] { CastElectionVote_(nullptr); }});

        auto pPopup = std::make_unique<ListSelectorPopup>(
            "Cast Ballot", "No candidates", std::move(choices), popupLayout,
            Style().listSelectorPopup, nullptr);
        m_elements.push_back(std::move(pPopup));
        return;
    }

    std::vector<PopupChoice_t> choices;
    choices.push_back({"Yea", [this] { CastBallot_(CouncilBallot_t::Yea); }});
    choices.push_back({"Nay", [this] { CastBallot_(CouncilBallot_t::Nay); }});
    choices.push_back({"Abstain", [this] { CastBallot_(CouncilBallot_t::Abstain); }});

    auto pPopup = std::make_unique<ListSelectorPopup>(
        "Cast Ballot", "No ballot options", std::move(choices), popupLayout,
        Style().listSelectorPopup, nullptr);
    m_elements.push_back(std::move(pPopup));
}

void CouncilVoteView::CastElectionVote_(Faction* pCandidate)
{
    m_rGameState.GetCouncilVotes().CastElectionVote(m_rGameState.GetPlayerFaction(), pCandidate);
    TryResolveAndClose_();
}

void CouncilVoteView::CastBallot_(CouncilBallot_t ballot)
{
    m_rGameState.GetCouncilVotes().CastVote(m_rGameState.GetPlayerFaction(), ballot);
    TryResolveAndClose_();
}

} // namespace ac
