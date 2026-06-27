#pragma once

#include "game/units/reactor/ReactorConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class ReactorRegistry : public Registry<ReactorConfig_t, ReactorConfigParser>
{
};

} // namespace ac
