#pragma once

#include "game/faction/base/production/ProductionApplyResult.h"
#include <string>

namespace ac
{

class BaseManager;
class BuildingRegistry;

// The end-of-turn completion state machine for one base's production queue.
//
//   Idle                    nothing queued
//   InProgress              queued but cost not met, DisableProduction, deferred, or a
//                           never-completing stockpile item
//   AwaitingAbandonConfirm  cost met, but finishing would empty the base
//   Completed               cost met and finished
//
// AwaitingAbandonConfirm and the deferral that answers it are the only state that persists
// across calls, because they are a question put to the player. They live here rather than in
// ProductionManager because deciding them needs population, buildings and unit designs, and
// rather than in BaseManager because they are the only turn-to-turn state that class would
// otherwise hold.
//
// Every route to completion — end of turn, a sibling prototype finishing, a mid-turn hurry —
// goes through TryCompleteReady, so the gate is stated once.
class ProductionCompletion
{
public:
    // rBase must outlive this object, which it does because BaseManager owns it. The building
    // registry is a constructor argument because deciding whether an item would empty the base
    // needs it, so the object is not usable without one.
    ProductionCompletion(BaseManager& rBase, const BuildingRegistry& rBuildings);
    ~ProductionCompletion() = default;

    // Stamp this turn's original item (BankProduction 0), then try to finish it. Adds no
    // minerals: ConvertMinerals already moved this turn's bank onto a real item.
    ProductionApplyResult_t Apply();

    // Finish the queued item if its cost is already met, without touching the mineral bank.
    ProductionApplyResult_t TryCompleteReady();

    // True while the player owes an answer to the abandon prompt.
    bool HasPendingAbandonConfirm() const;

    // Finish the item and accept the pop cost. Returns the completed item's id.
    std::string ConfirmAbandon();

    // Keep the base: the item stays queued with its invested minerals intact, and completion
    // is frozen until the queue changes — the same freeze shape as DisableProduction.
    void DeferAbandon();

    // Switching or clearing the queue cancels an unresolved prompt and lifts the freeze.
    void NotifyProductionChanged();

    // Whether the queued item is a prototype, which changes both its cost and the pause event
    // it reports. Public because GetMineralCost needs the same answer.
    bool IsCurrentPrototype() const;

private:
    // True when finishing the queued item would take the base to size 0 or below.
    bool WouldEmptyBase_() const;

    BaseManager& m_rBase;
    const BuildingRegistry& m_rBuildings;
    // Set when TryCompleteReady is ready to finish an item that would empty the base. Cleared
    // by ConfirmAbandon / DeferAbandon, or when production changes.
    bool m_bPendingAbandonConfirm = false;
    // Set by DeferAbandon: keep the queue and stockpile, but neither complete nor re-prompt
    // until the player changes production.
    bool m_bAbandonDeferred = false;
};

} // namespace ac
