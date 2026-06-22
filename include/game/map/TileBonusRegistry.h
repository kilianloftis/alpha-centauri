#pragma once

#include "game/map/TileBonusConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class TileBonusRegistry : public Registry<TileBonusConfig, TileBonusConfigParser>
{
};

} // namespace ac
