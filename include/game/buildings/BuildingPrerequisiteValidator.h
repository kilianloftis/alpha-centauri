#pragma once

namespace ac
{

struct GameDataContext;

// Every required_buildings entry id must name a loaded building, and must not be the declaring
// building. Throws when buildingRegistry is null.
void ValidateBuildingPrerequisiteReferences(const GameDataContext& rData);

} // namespace ac
