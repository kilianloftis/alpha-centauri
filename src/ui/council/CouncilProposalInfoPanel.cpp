#include "ui/council/CouncilProposalInfoPanel.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
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

    std::string tallyLine;
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        std::map<FactionId_t, int> tallies;
        int abstain = 0;
        for (Faction* pMember : pCouncil->Members())
        {
            if (!pMember)
            {
                continue;
            }
            const int weight = m_weights.Get(*pCouncil, *pMember, rConfig.voteWeight);
            const auto it = pPending->electionVotes.find(pMember->GetFactionId());
            if (it == pPending->electionVotes.end())
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

        for (Faction* pMember : pCouncil->Members())
        {
            if (!pMember)
            {
                continue;
            }
            const auto it = tallies.find(pMember->GetFactionId());
            const int votes = it == tallies.end() ? 0 : it->second;
            if (!tallyLine.empty())
            {
                tallyLine += "  ";
            }
            tallyLine += pMember->GetDefinition().identity.name + ": " + std::to_string(votes);
        }
        if (!tallyLine.empty())
        {
            tallyLine += "  ";
        }
        tallyLine += "Abstain: " + std::to_string(abstain);
    }
    else
    {
        int yea = 0;
        int nay = 0;
        int abstain = 0;
        for (Faction* pMember : pCouncil->Members())
        {
            if (!pMember)
            {
                continue;
            }
            const auto it = pPending->ballots.find(pMember->GetFactionId());
            if (it == pPending->ballots.end())
            {
                continue;
            }
            const int weight = m_weights.Get(*pCouncil, *pMember, rConfig.voteWeight);
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
        tallyLine = "Yea: " + std::to_string(yea) + "  Nay: " + std::to_string(nay)
                    + "  Abstain: " + std::to_string(abstain);
    }

    rGraphics.DrawText(
        tallyLine,
        m_layout.x + padX,
        m_layout.y + padY + 2.0f * lineHeight,
        style.tallyFontSize,
        style.tallyColor);
}

} // namespace ac
