#pragma once

#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class UnitComponentRegistry : public Registry<UnitComponentConfig_t, UnitComponentConfigParser>
{
};

} // namespace ac
