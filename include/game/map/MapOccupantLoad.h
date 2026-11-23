#pragma once

#include <string>

namespace ac
{

class ImprovementRegistry;
class TerraformProjectRegistry;

// Installs improvements.json + terrain.json occupants and terraform_projects.json Former
// projects. Marks place-targets for @buildable; validates places / blocked_by ids.
void LoadMapOccupants(const std::string& rImprovementsPath, const std::string& rTerrainPath,
                      const std::string& rTerraformProjectsPath, ImprovementRegistry& rOccupants,
                      TerraformProjectRegistry& rProjects);

} // namespace ac
