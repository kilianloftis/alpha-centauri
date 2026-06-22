#pragma once

#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/Registry.h"
#include <string>

namespace ac
{

class PopTypeRegistry : public Registry<PopTypeConfig, PopTypeConfigParser, Pop>
{
};

} // namespace ac
