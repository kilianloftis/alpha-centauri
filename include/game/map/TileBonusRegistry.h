#pragma once

#include "game/map/TileBonusConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class TileBonusRegistry : public Registry<TileBonusConfig_t, TileBonusConfig_tParser>
{
};

} // namespace ac
