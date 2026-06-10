#pragma once

#include <memory>
#include <string>

namespace ac
{

class Building;
class BuildingRegistry;

// BuildingFactory handles creation of individual Building instances from config.
class BuildingFactory
{
public:
    BuildingFactory();
    ~BuildingFactory();

    // Inject the registry used to look up building definitions.
    void SetRegistry(const BuildingRegistry* pRegistry);

    // Create a building of the given id.
    // Returns nullptr if the id is not found in the registry.
    std::unique_ptr<Building> CreateBuilding(const std::string& buildingId) const;

private:
    const BuildingRegistry* m_pRegistry = nullptr;
};

} // namespace ac
