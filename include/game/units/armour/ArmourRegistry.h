#pragma once

#include "game/units/armour/ArmourConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class ArmourRegistry : public Registry<ArmourConfig_t, ArmourConfigParser>
{
};

} // namespace ac
