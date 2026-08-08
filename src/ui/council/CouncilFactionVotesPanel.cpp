#include "ui/council/CouncilFactionVotesPanel.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/PlanetaryCouncil.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <string>

namespace ac
{
namespace
{

std::string BallotLabel_(CouncilBallot_t ballot)
{
    switch (ballot)
    {
    case CouncilBallot_t::Yea:
        return "Yea";
    case CouncilBallot_t::Nay:
        return "Nay";
    case CouncilBallot_t::Abstain:
        return "Abstain";
    }
    return "—";
}

std::string MemberVoteLabel_(const PlanetaryCouncil& rCouncil,
                             const PlanetaryCouncil::PendingProposal_t& rPending,
                             const CouncilProposalConfig_t& rConfig,
                             const Faction& rMember)
{
    const FactionId_t id = rMember.GetFactionId();
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        const auto it = rPending.electionVotes.find(id);
        if (it == rPending.electionVotes.end())
        {
            return "—";
        }
        if (!it->second)
        {
            return "Abstain";
        }
        for (const Faction* pCandidate : rCouncil.Members())
        {
            if (pCandidate && pCandidate->GetFactionId() == *it->second)
            {
                return pCandidate->GetDefinition().identity.name;
            }
        }
        return "—";
    }

    const auto it = rPending.ballots.find(id);
    if (it == rPending.ballots.end())
    {
        return "—";
    }
    return BallotLabel_(it->second);
}

} // namespace

CouncilFactionVotesPanel::CouncilFactionVotesPanel(GameState& rGameState, WindowLayout_t layout)
    : UIElement(layout)
    , m_rGameState(rGameState)
{
}

void CouncilFactionVotesPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().councilVoteView;

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    const PlanetaryCouncil* pCouncil = m_rGameState.GetPlanetaryCouncil();
    if (!pCouncil)
    {
        return;
    }
    const PlanetaryCouncil::PendingProposal_t* pPending = pCouncil->GetPending();
    if (!pPending)
    {
        return;
    }

    const CouncilProposalConfig_t& rConfig = pCouncil->GetRegistry().Get(pPending->proposalId);
    const std::vector<Faction*>& rMembers = pCouncil->Members();
    if (rMembers.empty())
    {
        return;
    }

    const float sliceWidth = m_layout.width / static_cast<float>(rMembers.size());
    const float padX = style.paddingRatio * sliceWidth;
    const float lineHeight = style.lineHeightRatio * m_layout.height;
    const float startY = m_layout.y + style.paddingRatio * m_layout.height;

    for (size_t i = 0; i < rMembers.size(); ++i)
    {
        Faction* pMember = rMembers[i];
        if (!pMember)
        {
            continue;
        }

        const float sliceX = m_layout.x + static_cast<float>(i) * sliceWidth;
        rGraphics.DrawRect(sliceX, m_layout.y, sliceWidth, m_layout.height, style.borderColor);

        const std::string& rName = pMember->GetDefinition().identity.name;
        const std::string voteLabel = MemberVoteLabel_(*pCouncil, *pPending, rConfig, *pMember);
        const int weight = m_weights.Get(*pCouncil, *pMember, rConfig.voteWeight);

        rGraphics.DrawText(
            rName,
            sliceX + padX,
            startY,
            style.factionFontSize,
            style.factionNameColor);
        rGraphics.DrawText(
            voteLabel,
            sliceX + padX,
            startY + lineHeight,
            style.ballotFontSize,
            style.ballotColor);

        if (rConfig.voteWeight == CouncilVoteWeight_t::Population || weight != 1)
        {
            rGraphics.DrawText(
                std::to_string(weight) + " votes",
                sliceX + padX,
                startY + 2.0f * lineHeight,
                style.weightFontSize,
                style.weightColor);
        }
    }
}

} // namespace ac
