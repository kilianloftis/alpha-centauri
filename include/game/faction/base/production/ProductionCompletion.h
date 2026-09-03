#pragma once

#include "game/faction/base/production/ProductionApplyResult.h"
#include <string>

namespace ac
{

class BaseManager;
class BuildingRegistry;

// The end-of-turn completion state machine for one base's production queue.
//
//   Idle           nothing queued
//   InProgress     queued but cost not met, production disabled, or a never-completing
//                  stockpile item
//   WouldEmptyBase cost met, but finishing would take the base to size <= 0
//   Completed      cost met and finished
//
// WouldEmptyBase is the only state that persists across calls, because it is a question put
// to the player. It lives here rather than in ProductionManager because deciding it needs
// population, buildings and unit designs, and rather than in BaseManager because it is the
// only turn-to-turn state that class would otherwise hold.
//
// After WouldEmptyBase_, this class does not know about razing or "abandon": complete is
// CompleteProduction (size 0 is Instantaneous + the Population stage); decline is
// BaseManager emitting the DisableProduction RuleFlag — the same flag riot already uses.
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

    // True while the player owes an answer to the would-empty prompt.
    bool HasPendingEmptyBaseChoice() const;

    // Finish the item. Instantaneous pop cost may take the base to size 0. Returns the
    // completed item's id.
    std::string CompletePending();

    // Accept the disable choice: clear the pending prompt. BaseManager then emits the
    // DisableProduction RuleFlag until the queue changes.
    void DisableProduction();

    // Switching or clearing the queue cancels an unresolved prompt.
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
    // by CompletePending / DisableProduction, or when production changes.
    bool m_bPendingEmptyBaseChoice = false;
};

} // namespace ac
