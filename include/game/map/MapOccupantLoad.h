#pragma once

#include <string>

namespace ac
{

class ImprovementRegistry;
class TerrainOperationRegistry;

// The one path that installs everything config/improvements.json and config/terrain.json
// describe: buildable improvements and terrain features into rOccupants (parsed and
// tag-expanded together, so an improvement may name a tag only terrain carries), and former
// projects into rOperations. terrain.json is read once and each half handed to its owner.
//
// Also rejects an operation id that a buildable improvement shadows — FindTerraformProject
// resolves improvements first, so such an id would silently never reach the operation.
// Production and the test fixtures both call this, so neither can drift from the other or
// skip the check. Callers with no use for former projects use
// ImprovementRegistry::LoadOccupants instead.
void LoadMapOccupants(const std::string& rImprovementsPath, const std::string& rTerrainPath,
                      ImprovementRegistry& rOccupants, TerrainOperationRegistry& rOperations);

} // namespace ac
