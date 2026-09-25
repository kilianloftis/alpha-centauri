#pragma once

#include "game/map/TerrainConfig.h"
#include "lib/Registry.h"

namespace ac
{

// Former projects that place no improvement. The set is open: an operation is defined by the
// triggered effects it runs, so a new one is a terrain.json entry rather than a code change.
// Normally assigned from TerrainFile_t rather than loaded as its own array file.
class TerrainOperationRegistry : public Registry<TerrainOperationConfig_t, TerrainOperationConfigParser>
{
};

} // namespace ac
