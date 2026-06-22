#pragma once

#include "game/buildings/Building.h"
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class BuildingRegistry;

// BuildingManager owns all Building instances for a single base.
// Buildings are added by id (looked up via BuildingRegistry) and destroyed by id.
// Provides aggregate bonus queries that sum across all held buildings.
class BuildingManager
{
public:
    explicit BuildingManager(const BuildingRegistry* pRegistry);
    ~BuildingManager();

    // Add a building by id. Throws if the factory cannot find the id.
    void AddBuilding(const std::string& buildingId);

    // Destroy the first building with the given id. No-op if not present.
    void DestroyBuilding(const std::string& buildingId);

    // All currently held buildings.
    const std::vector<std::unique_ptr<Building>>& GetBuildings() const;

    // Sum of GetNutrientsBonus() across all buildings.
    int GetTotalNutrientsBonus() const;

    // Get a list of buildings that can be constructed at this base.
    // discoveredBuildings is the faction-level list of tech-unlocked buildings.
    // Base-local rules (allowMultiple, already built) are applied here.
    std::vector<const Building*> GetBuildingsAvailableForConstruction(const std::vector<const Building*>& discoveredBuildings) const;

private:
    bool DoesBuildingExist_(const std::string& buildingId) const;

    const BuildingRegistry* m_pRegistry;
    std::vector<std::unique_ptr<Building>> m_buildings;
};

} // namespace ac
