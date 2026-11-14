#pragma once

#include "game/buildings/RequiredBuildings.h"
#include "lib/config/ConfigReader.h"

namespace ac
{

// Parses a required_buildings object (any / all / none entry arrays). Absent key → empty.
RequiredBuildings_t ParseRequiredBuildings(const ConfigReader& rReader, const char* pKey);

} // namespace ac
