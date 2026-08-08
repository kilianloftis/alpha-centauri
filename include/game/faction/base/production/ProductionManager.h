#pragma once

#include "game/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "lib/Signal.h"
#include <string>

namespace ac
{

// ProductionManager is the API surface for the production component.
// A base can build one item at a time.
// TODO: Extend to unit production once unit definitions with mineral costs exist.
class ProductionManager
{
public:
    ProductionManager();
    ~ProductionManager();

    // Set the item to produce; nullptr clears it. Setting the item already queued is a no-op
    // and does not re-announce a change.
    //
    // Contract, stated because it was not: **the mineral stockpile is never touched here.**
    // Switching carries the full stockpile to the new item, and clearing keeps it for whatever
    // is queued next. Completion (ApplyProduction) is the only thing that spends it.
    // TODO: SMAC penalises switching between production categories (unit / facility / project).
    // That rule is not implemented; carrying the full stockpile is the placeholder, not a
    // decision. It needs the penalty rule before it can be anything else.
    //
    // The item is not validated against what this base may actually build — BaseManager owns
    // that question (GetConstructable), and its availability calculator is optional, so the
    // check cannot live here.
    void SetProduction(const IConstructable* pItem);

    // The item currently being produced, or nullptr if none.
    const IConstructable* GetCurrentProduction() const;

    // True if a production item is currently set.
    bool HasProduction() const;

    // Effective mineral cost of the current production item after CostMultiplier effects
    // in rBaseEffects (e.g. Industry social-rating levels). Returns 0 when nothing is queued.
    int GetMineralCost(const BaseEffects_t& rBaseEffects) const;

    // Mineral stockpile owned by this manager.
    int GetMineralStockpile() const;
    void SetMineralStockpile(int amount);

    // Apply minerals produced this turn: add to stockpile, complete if cost is met.
    // rBaseEffects is forwarded to GetMineralCost. Returns the completed item id,
    // or empty string if construction is ongoing.
    std::string ApplyProduction(int minerals, const BaseEffects_t& rBaseEffects);

    // Complete the current production immediately and return its id.
    std::string CompleteProduction();

    // Emitted when a production item is completed, with the completed item id.
    Signal<std::string> OnProductionCompleted;

    // Emitted when the current production item changes (including on clear).
    Signal<> OnProductionChanged;

private:
    const IConstructable* m_pCurrentItem = nullptr;
    int m_mineralStockpile = 0;

    void ResetProduction_();
};

} // namespace ac
