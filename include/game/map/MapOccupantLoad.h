#pragma once

#include <string>

namespace ac
{

class ImprovementRegistry;
class TerrainOperationRegistry;

// Installs everything improvements.json and terrain.json describe: occupants and former
// projects. Rejects an operation id a buildable improvement would shadow.
void LoadMapOccupants(const std::string& rImprovementsPath, const std::string& rTerrainPath,
                      ImprovementRegistry& rOccupants, TerrainOperationRegistry& rOperations);

} // namespace ac
