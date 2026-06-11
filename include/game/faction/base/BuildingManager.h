#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ac
{

class Building;
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

    // Sum of GetImprovementNutrientsBonus(improvementName) across all buildings.
    int GetTotalImprovementNutrientsBonus(const std::string& improvementName) const;

private:
    const BuildingRegistry* m_pRegistry;
    std::vector<std::unique_ptr<Building>> m_buildings;
};

} // namespace ac
