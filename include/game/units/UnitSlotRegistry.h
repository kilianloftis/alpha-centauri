#pragma once

#include "game/units/UnitSlotConfig.h"
#include "game/units/UnitSlotConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class UnitSlotRegistry : public Registry<UnitSlotConfig_t, UnitSlotConfigParser>
{
};

} // namespace ac
