#include "game/faction/base/production/ProductionCompletion.h"

#include <stdexcept>

namespace ac
{

ProductionCompletion::EvaluateResult_t
ProductionCompletion::MakeReadyToFinish_(const ProductionCompletionProbe_t& probe)
{
    if (!probe.hasProduction)
    {
        throw std::logic_error("ProductionCompletion::MakeReadyToFinish_: nothing queued");
    }
    return EvaluateResult_t{Outcome_t::ReadyToFinish, probe.isPrototype};
}

ProductionCompletion::EvaluateResult_t
ProductionCompletion::TryCompleteReady(const ProductionCompletionProbe_t& probe, bool bNewTurn)
{
    if (bNewTurn)
    {
        // Last turn's "not this turn" does not answer for this turn.
        m_bDeferredThisTurn = false;
    }

    if (m_bPendingConfirmation)
    {
        return EvaluateResult_t{Outcome_t::AwaitingConfirmation, false};
    }

    if (!probe.hasProduction)
    {
        return EvaluateResult_t{Outcome_t::Idle, false};
    }
    if (probe.productionDisabled)
    {
        // Riot: the base produces nothing this turn. The stockpile is untouched and completion
        // stays blocked until the DisableProduction RuleFlag lifts. BaseManager::ApplyProduction
        // also early-outs on riot before banking leftovers — both sites are required.
        return EvaluateResult_t{Outcome_t::InProgress, false};
    }

    if (!probe.readyToComplete)
    {
        return EvaluateResult_t{Outcome_t::InProgress, false};
    }

    if (probe.wouldAbandonBase)
    {
        if (m_bDeferredThisTurn)
        {
            return EvaluateResult_t{Outcome_t::InProgress, false};
        }
        m_bPendingConfirmation = true;
        return EvaluateResult_t{Outcome_t::AwaitingConfirmation, false};
    }

    return MakeReadyToFinish_(probe);
}

bool ProductionCompletion::HasPendingConfirmation() const
{
    return m_bPendingConfirmation;
}

bool ProductionCompletion::IsCompletionBlocked() const
{
    return m_bPendingConfirmation || m_bDeferredThisTurn;
}

ProductionCompletion::EvaluateResult_t
ProductionCompletion::AcceptPending(const ProductionCompletionProbe_t& probe)
{
    if (!m_bPendingConfirmation)
    {
        throw std::runtime_error(
            "ProductionCompletion::AcceptPending: no answer is outstanding");
    }
    // Clear before the owner spends the queue: ResetProduction_ emits OnProductionChanged
    // which would also clear the flag, but AcceptPending must own the transition explicitly.
    m_bPendingConfirmation = false;
    return MakeReadyToFinish_(probe);
}

void ProductionCompletion::DeferCompletion()
{
    if (!m_bPendingConfirmation)
    {
        throw std::runtime_error(
            "ProductionCompletion::DeferCompletion: no answer is outstanding");
    }
    m_bPendingConfirmation = false;
    m_bDeferredThisTurn = true;
}

void ProductionCompletion::NotifyProductionChanged()
{
    m_bPendingConfirmation = false;
    m_bDeferredThisTurn = false;
}

} // namespace ac
