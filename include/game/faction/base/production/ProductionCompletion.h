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

// The completion state machine for one base's production queue.
//
//   Idle                 nothing queued
//   InProgress           cost not met, production disabled by riot, or completion already
//                        deferred this turn
//   AwaitingConfirmation cost met, but finishing needs an answer from the player
//   ReadyToFinish        cost met and allowed — owner must spend the queue and pack the
//                        player-facing ProductionApplyResult (id + pause gates + name)
//
// This class does not know *why* an item needs confirming — the probe's wouldAbandonBase is
// whatever BaseManager computed. Deferring is a one-turn answer about one item, not a state
// of the base: a new-turn TryCompleteReady clears it and asks again. That makes it a different
// thing from riot's DisableProduction, which stops the base producing at all.
//
// Pause-on-event classification is CollectProductionPauseGates, not this type.
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

    // Finish the queued item if its cost is already met. bNewTurn clears last turn's deferral
    // before checking (turn tick); mid-turn callers pass false.
    EvaluateResult_t TryCompleteReady(const ProductionCompletionProbe_t& probe, bool bNewTurn);

    // True while the player owes an answer.
    bool HasPendingConfirmation() const;

    // True while the queued item is funded but will not finish: an answer is outstanding, or
    // one was already given this turn. Distinct from BaseManager::IsProductionDisabled.
    bool IsCompletionBlocked() const;

    // Answer "finish it anyway". Clears pending and returns ReadyToFinish; the owner then
    // spends the queue. Throws if nothing is pending. probe.isPrototype must still describe
    // the funded item.
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
    // The answer was "not this turn". Cleared by TryCompleteReady(bNewTurn=true).
    bool m_bDeferredThisTurn = false;
};

} // namespace ac
