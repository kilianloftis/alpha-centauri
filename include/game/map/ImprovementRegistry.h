#pragma once

#include "game/map/ImprovementConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class ImprovementRegistry : public Registry<ImprovementConfig_t, ImprovementConfigParser>
{
};

} // namespace ac
