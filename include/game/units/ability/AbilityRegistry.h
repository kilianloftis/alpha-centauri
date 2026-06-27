#pragma once

#include "game/units/ability/AbilityConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class AbilityRegistry : public Registry<AbilityConfig_t, AbilityConfigParser>
{
};

} // namespace ac
