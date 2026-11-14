#include "game/buildings/RequiredBuildingsParser.h"

#include "game/RequirementListsParser.h"

namespace ac
{

namespace
{

BuildingRequirement_t ParseBuildingRequirementEntry_(const ConfigReader& rEntry)
{
    rEntry.RejectUnknownKeys({"building", "scope", "count"});
    BuildingRequirement_t entry;
    entry.building = rEntry.NonEmptyString("building");
    entry.scope = rEntry.Enum<BuildingRequirementScope_t>("scope", "building requirement scope");
    if (rEntry.Has("count"))
    {
        entry.count = rEntry.IntAtLeast("count", 1);
    }
    return entry;
}

} // namespace

RequiredBuildings_t ParseRequiredBuildings(const ConfigReader& rReader, const char* pKey)
{
    return ParseRequirementLists<BuildingRequirement_t>(
        rReader, pKey, ParseBuildingRequirementEntry_, {"any", "all", "none"});
}

} // namespace ac
