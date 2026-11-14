#include "game/buildings/BuildingPrerequisiteValidator.h"

#include "game/GameDataContext.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/RequiredBuildings.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

void ValidateEntryList_(const BuildingConfig_t& rBuilding,
                        const std::vector<BuildingRequirement_t>& rEntries, const char* pListName,
                        const BuildingRegistry& rRegistry)
{
    for (const BuildingRequirement_t& rEntry : rEntries)
    {
        if (rEntry.building == rBuilding.id)
        {
            throw std::runtime_error(
                "Building '" + rBuilding.id + "' required_buildings." + pListName
                + " must not include itself");
        }
        if (!rRegistry.Find(rEntry.building))
        {
            throw std::runtime_error(
                "Building '" + rBuilding.id + "' required_buildings." + pListName
                + " has unknown building '" + rEntry.building + "'");
        }
    }
}

} // namespace

void ValidateBuildingPrerequisiteReferences(const GameDataContext& rData)
{
    if (!rData.buildingRegistry)
    {
        throw std::runtime_error(
            "ValidateBuildingPrerequisiteReferences: GameDataContext.buildingRegistry is null");
    }
    const BuildingRegistry& rRegistry = *rData.buildingRegistry;
    for (const BuildingConfig_t& rBuilding : rRegistry.GetAll())
    {
        const RequiredBuildings_t& rRequired = rBuilding.requiredBuildings;
        ValidateEntryList_(rBuilding, rRequired.any, "any", rRegistry);
        ValidateEntryList_(rBuilding, rRequired.all, "all", rRegistry);
        ValidateEntryList_(rBuilding, rRequired.none, "none", rRegistry);
    }
}

} // namespace ac
