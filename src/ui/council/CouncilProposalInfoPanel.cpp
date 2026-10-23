#include "ui/council/CouncilProposalInfoPanel.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/council/PlanetaryCouncil.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <map>
#include <string>

namespace ac
{

CouncilProposalInfoPanel::CouncilProposalInfoPanel(GameState& rGameState, WindowLayout_t layout)
    : UIElement(layout)
    , m_rGameState(rGameState)
{
}

void CouncilProposalInfoPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().councilVoteView;

    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        style.borderColor, Style().outline.width);

    const PlanetaryCouncil& rCouncil = m_rGameState.GetPlanetaryCouncil();
    const PendingProposal_t* pPending = m_rGameState.GetCouncilVotes().GetPending();
    if (!pPending)
    {
        return;
    }

    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(pPending->proposalId);
    const float padX = style.paddingRatio * m_layout.width;
    const float padY = style.paddingRatio * m_layout.height;
    const float lineHeight = style.lineHeightRatio * m_layout.height;

    rGraphics.DrawText(
        "Proposal",
        m_layout.x + padX,
        m_layout.y + padY,
        style.headerFontSize,
        style.headerColor);
    rGraphics.DrawText(
        rConfig.name,
        m_layout.x + padX,
        m_layout.y + padY + lineHeight,
        style.nameFontSize,
        style.nameColor);

    const std::string tallyLine = rConfig.kind == CouncilProposalKind_t::Election
                                      ? ElectionTally_(rCouncil, *pPending, rConfig)
                                      : BallotTally_(rCouncil, *pPending, rConfig);
    rGraphics.DrawText(
        tallyLine,
        m_layout.x + padX,
        m_layout.y + padY + 2.0f * lineHeight,
        style.tallyFontSize,
        style.tallyColor);
}

std::string CouncilProposalInfoPanel::ElectionTally_(const PlanetaryCouncil& rCouncil,
                                                     const PendingProposal_t& rPending,
                                                     const CouncilProposalConfig_t& rConfig)
{
    std::map<FactionId_t, int> tallies;
    int abstain = 0;
    for (Faction* pMember : rCouncil.Members())
    {
        const int weight = m_weights.Get(rCouncil, *pMember, rConfig.voteWeight);
        const auto it = rPending.electionVotes.find(pMember->GetFactionId());
        if (it == rPending.electionVotes.end())
        {
            continue;
        }
        if (!it->second)
        {
            abstain += weight;
            continue;
        }
        tallies[*it->second] += weight;
    }

    std::string tallyLine;
    for (Faction* pMember : rCouncil.Members())
    {
        const auto it = tallies.find(pMember->GetFactionId());
        const int votes = it == tallies.end() ? 0 : it->second;
        tallyLine += pMember->GetDefinition().identity.name + ": " + std::to_string(votes) + "  ";
    }
    return tallyLine + "Abstain: " + std::to_string(abstain);
}

std::string CouncilProposalInfoPanel::BallotTally_(const PlanetaryCouncil& rCouncil,
                                                   const PendingProposal_t& rPending,
                                                   const CouncilProposalConfig_t& rConfig)
{
    int yea = 0;
    int nay = 0;
    int abstain = 0;
    for (Faction* pMember : rCouncil.Members())
    {
        const auto it = rPending.ballots.find(pMember->GetFactionId());
        if (it == rPending.ballots.end())
        {
            continue;
        }
        const int weight = m_weights.Get(rCouncil, *pMember, rConfig.voteWeight);
        switch (it->second)
        {
        case CouncilBallot_t::Yea:
            yea += weight;
            break;
        case CouncilBallot_t::Nay:
            nay += weight;
            break;
        case CouncilBallot_t::Abstain:
            abstain += weight;
            break;
        }
    }
    return "Yea: " + std::to_string(yea) + "  Nay: " + std::to_string(nay)
           + "  Abstain: " + std::to_string(abstain);
}

} // namespace ac
