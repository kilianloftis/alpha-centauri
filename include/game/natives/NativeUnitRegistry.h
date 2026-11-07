#pragma once

#include "game/natives/NativeUnitConfig.h"
#include "game/natives/NativeUnitConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class NativeUnitRegistry : public Registry<NativeUnitConfig_t, NativeUnitConfigParser>
{
};

} // namespace ac
