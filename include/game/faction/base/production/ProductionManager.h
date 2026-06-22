#pragma once

#include "game/buildings/BuildingRegistry.h"
#include "lib/Signal.h"
#include <string>

namespace ac
{

class BuildingRegistry;

// ProductionManager is the API surface for the production component.
// A base can build one item at a time. Items are identified by their config id
// and must have a mineral cost (currently buildings via BuildingRegistry).
// TODO: Extend to unit production once unit definitions with mineral costs exist.
class ProductionManager
{
public:
    explicit ProductionManager(const BuildingRegistry* pBuildingRegistry);
    ~ProductionManager();

    // Set the item being produced.
    // Throws if the item is unknown or has no associated cost registry.
    void SetProduction(const std::string& itemId);

    // Get the id of the item currently being produced.
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
    const BuildingRegistry* m_pBuildingRegistry;
    std::string m_currentItemId;

    void ResetProduction_();
    const BuildingConfig_t* FindConfig_() const;
};

} // namespace ac
