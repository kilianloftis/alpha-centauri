#pragma once

#include "game/buildings/BuildingConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class BuildingRegistry : public Registry<BuildingConfig_t, BuildingConfigParser>
{
};

} // namespace ac
