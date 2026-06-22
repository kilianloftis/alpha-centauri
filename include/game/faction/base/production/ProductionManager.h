#pragma once

#include "game/buildings/Building.h"
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

    // Set the building to produce. Pass nullptr to clear production.
    void SetProduction(const Building* pBuilding);

    // The building currently being produced, or nullptr if none.
    const Building* GetCurrentBuilding() const;

    // Get the id of the item currently being produced, or empty string if none.
    const std::string& GetProduction() const;

    // True if a production item is currently set.
    bool HasProduction() const;

    // Total mineral cost of the current production item.
    int GetMineralCost() const;

    // Complete the current production immediately and return its id.
    std::string CompleteProduction();

    // Emitted when a production item is completed, with the completed item id.
    Signal<std::string> on_production_completed;

    // Emitted when the current production item changes (including on clear).
    Signal<> on_production_changed;

private:
    const Building* m_pCurrentBuilding = nullptr;

    void ResetProduction_();
};

} // namespace ac
