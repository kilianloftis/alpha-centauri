#pragma once

#include "game/IConstructable.h"
#include "game/faction/base/production/ProductionCostCalculator.h"
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
    explicit ProductionManager(const ProductionCostCalculator& rCostCalculator);
    ~ProductionManager();

    // Set the item to produce. Pass nullptr to clear production.
    void SetProduction(const IConstructable* pItem);

    // The item currently being produced, or nullptr if none.
    const IConstructable* GetCurrentProduction() const;

    // True if a production item is currently set.
    bool HasProduction() const;

    // Effective mineral cost of the current production item (after calculator modifiers).
    int GetMineralCost() const;

    // Mineral stockpile owned by this manager.
    int GetMineralStockpile() const;

    // Apply minerals produced this turn: add to stockpile, complete if cost is met.
    // Returns the completed item id, or empty string if construction is ongoing.
    std::string ApplyProduction(int minerals);

    // Complete the current production immediately and return its id.
    std::string CompleteProduction();

    // Emitted when a production item is completed, with the completed item id.
    Signal<std::string> on_production_completed;

    // Emitted when the current production item changes (including on clear).
    Signal<> on_production_changed;

private:
    const ProductionCostCalculator* m_pCostCalculator = nullptr;
    const IConstructable* m_pCurrentItem = nullptr;
    int m_mineralStockpile = 0;

    void ResetProduction_();
};

} // namespace ac
