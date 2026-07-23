#pragma once

#include "game/map/WorldGenPresetConfig.h"
#include "game/map/WorldGenPresetConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class WorldGenPresetRegistry : public Registry<WorldGenPresetConfig_t, WorldGenPresetConfigParser>
{
};

} // namespace ac
