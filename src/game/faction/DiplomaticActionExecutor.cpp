#include "game/faction/DiplomaticActionExecutor.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomaticProposalEffects.h"
#include "game/faction/DiplomaticProposalRules.h"

namespace ac
{

DiplomaticProposeResult_t DiplomaticActionExecutor::Propose(GameState& rState,
                                                          const DiplomaticProposal_t& rProposal)
{
    if (!IsValidProposal(rState, rProposal))
    {
        return DiplomaticProposeResult_t::Invalid;
    }

    if (rState.RequireFaction(rProposal.recipient).IsPlayerControlled())
    {
        if (m_pending.has_value())
        {
            return DiplomaticProposeResult_t::Busy;
        }
        m_pending = rProposal;
        return DiplomaticProposeResult_t::PendingPlayer;
    }

    if (!EvaluateResponse_(rState, rProposal.recipient))
    {
        return DiplomaticProposeResult_t::Rejected;
    }

    ApplyProposal(rState, rProposal);
    return DiplomaticProposeResult_t::Accepted;
}

bool DiplomaticActionExecutor::Accept(GameState& rState)
{
    if (!m_pending.has_value())
    {
        return false;
    }
    DiplomaticProposal_t proposal = *m_pending;
    m_pending.reset();
    if (!IsValidProposal(rState, proposal))
    {
        return false;
    }
    ApplyProposal(rState, proposal);
    return true;
}

void DiplomaticActionExecutor::Reject()
{
    m_pending.reset();
}

bool DiplomaticActionExecutor::EvaluateResponse_(const GameState& /*rState*/,
                                                 FactionId_t /*recipientId*/) const
{
    // TODO: AI attitude model. Until it exists every AI accepts. The relationship should weigh
    // on the answer: a recipient at Vendetta with the proposer is less likely to accept.
    return true;
}

} // namespace ac
