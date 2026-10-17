#pragma once

#include "game/worldgen/WorldGenPresetConfig.h"
#include "game/worldgen/WorldGenPresetConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class WorldGenPresetRegistry : public Registry<WorldGenPresetConfig_t, WorldGenPresetConfigParser>
{
};

} // namespace ac
