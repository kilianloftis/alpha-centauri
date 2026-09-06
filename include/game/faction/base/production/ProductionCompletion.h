#pragma once

#include "game/faction/base/production/ProductionApplyResult.h"
#include <string>

namespace ac
{

class BaseManager;

// The end-of-turn completion state machine for one base's production queue.
//
//   Idle                 nothing queued
//   InProgress           cost not met, a never-completing stockpile item, production disabled
//                        by riot, or completion already deferred this turn
//   AwaitingConfirmation cost met, but finishing needs an answer from the player
//   Completed            cost met and finished
//
// This class does not know *why* an item needs confirming — it asks
// BaseManager::WouldCompletionAbandonBase and reports the answer. Deferring is a one-turn
// answer about one item, not a state of the base: Apply clears it each turn and asks again,
// so a base that has since grown simply completes. That makes it a different thing from riot's
// DisableProduction, which stops the base producing at all and which this class only reads.
//
// Every route to completion — end of turn, a sibling prototype finishing, a mid-turn hurry —
// goes through TryCompleteReady, so the gate is stated once.
class ProductionCompletion
{
public:
    // rBase must outlive this object, which it does because BaseManager owns it.
    explicit ProductionCompletion(BaseManager& rBase);
    ~ProductionCompletion() = default;

    // Stamp this turn's original item (BankProduction 0), drop last turn's deferral, then try
    // to finish. Adds no minerals: ConvertMinerals already moved this turn's bank onto a real
    // item. The stamp happens even while an answer is outstanding, so switching away from a
    // deferred item still charges retool.
    ProductionApplyResult_t Apply();

    // Finish the queued item if its cost is already met, without touching the mineral bank.
    ProductionApplyResult_t TryCompleteReady();

    // True while the player owes an answer.
    bool HasPendingConfirmation() const;

    // True while the queued item is funded but will not finish: an answer is outstanding, or
    // one was already given this turn. Distinct from BaseManager::IsProductionDisabled, which
    // is riot stopping the base producing at all.
    bool IsCompletionBlocked() const;

    // Answer "finish it anyway". Returns the completed item's id. Throws if nothing is pending.
    std::string CompletePending();

    // Answer "not this turn": the item stays queued and funded, and is asked about again next
    // turn. Throws if nothing is pending.
    void DeferCompletion();

    // Switching or clearing the queue cancels an unresolved answer and the deferral with it.
    void NotifyProductionChanged();

    // Whether the queued item is a prototype, which changes both its cost and the pause event
    // it reports. Public because GetMineralCost needs the same answer.
    bool IsCurrentPrototype() const;

private:
    BaseManager& m_rBase;
    // An answer is outstanding. Cleared by CompletePending / DeferCompletion, or when
    // production changes.
    bool m_bPendingConfirmation = false;
    // The answer was "not this turn". Cleared by Apply, so the question is re-asked next turn
    // against whatever the base looks like then.
    bool m_bDeferredThisTurn = false;
};

} // namespace ac
