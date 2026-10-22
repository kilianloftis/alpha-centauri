#pragma once

#include "game/map/TerrainConfig.h"
#include "lib/Registry.h"

namespace ac
{

// Former projects that place no improvement. Assigned from TerrainFile_t.
class TerrainOperationRegistry : public Registry<TerrainOperationConfig_t, TerrainOperationConfigParser>
{
};

} // namespace ac
