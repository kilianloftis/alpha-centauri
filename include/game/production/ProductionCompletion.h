#pragma once

namespace ac
{

class IConstructable;

// Inputs for one completion evaluation. The owner (ProductionManager) gathers these from the
// base and the queue; ProductionCompletion never holds either manager.
struct ProductionCompletionProbe_t
{
    bool hasProduction = false;
    bool productionDisabled = false;
    bool readyToComplete = false;
    bool wouldAbandonBase = false;
    bool isPrototype = false;
};

// Completion for one base's queue: Idle, InProgress, AwaitingConfirmation, ReadyToFinish. A
// deferral lasts one turn; riot's DisableProduction is a different thing.
class ProductionCompletion
{
public:
    // Gate outcome before the owner spends the queue.
    enum class Outcome_t
    {
        Idle,
        InProgress,
        AwaitingConfirmation,
        ReadyToFinish,
    };

    struct EvaluateResult_t
    {
        Outcome_t outcome = Outcome_t::Idle;
        // Meaningful when outcome is ReadyToFinish (and after AcceptPending).
        bool isPrototype = false;
    };

    ProductionCompletion() = default;
    ~ProductionCompletion() = default;

    // Last turn's "not this turn" does not answer for this one.
    void StartTurn();
    // Finish the queued item if its cost is already met.
    EvaluateResult_t TryCompleteReady(const ProductionCompletionProbe_t& probe);

    // True while the player owes an answer.
    bool HasPendingConfirmation() const;

    // True while the queued item is funded but will not finish: an answer is outstanding, or
    // one was already given this turn. Distinct from BaseManager::IsProductionDisabled.
    bool IsCompletionBlocked() const;

    // "Finish it anyway": clears pending and returns ReadyToFinish. Throws if nothing is pending.
    EvaluateResult_t AcceptPending(const ProductionCompletionProbe_t& probe);

    // Answer "not this turn": the item stays queued and funded, and is asked about again next
    // turn. Throws if nothing is pending.
    void DeferCompletion();

    // Switching or clearing the queue cancels an unresolved answer and the deferral with it.
    void NotifyProductionChanged();

private:
    static EvaluateResult_t MakeReadyToFinish_(const ProductionCompletionProbe_t& probe);

    // An answer is outstanding. Cleared by AcceptPending / DeferCompletion, or when
    // production changes.
    bool m_bPendingConfirmation = false;
    // The answer was "not this turn". Cleared by StartTurn.
    bool m_bDeferredThisTurn = false;
};

} // namespace ac
