#pragma once

#include "game/units/NativeUnitConfig.h"
#include "game/units/NativeUnitConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class NativeUnitRegistry : public Registry<NativeUnitConfig_t, NativeUnitConfigParser>
{
};

} // namespace ac
