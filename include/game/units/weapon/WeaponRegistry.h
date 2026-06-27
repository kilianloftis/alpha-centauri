#pragma once

#include "game/units/weapon/WeaponConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class WeaponRegistry : public Registry<WeaponConfig_t, WeaponConfigParser>
{
};

} // namespace ac
