#pragma once

#include "game/faction/FactionConfig.h"
#include "game/faction/FactionConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class FactionRegistry : public Registry<FactionConfig_t, FactionConfigParser>
{
};

} // namespace ac
