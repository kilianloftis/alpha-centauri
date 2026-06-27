#pragma once

#include "game/units/chassis/ChassisConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class ChassisRegistry : public Registry<ChassisConfig_t, ChassisConfigParser>
{
};

} // namespace ac
