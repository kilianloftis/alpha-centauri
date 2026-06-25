#pragma once

#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class PopTypeRegistry : public Registry<PopTypeConfig_t, PopTypeConfig_tParser, Pop>
{
};

} // namespace ac
