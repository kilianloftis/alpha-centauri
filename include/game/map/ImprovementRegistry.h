#pragma once

#include "game/map/ImprovementConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

// Every tile occupant, improvement or terrain, in one id space so no id can be both.
class ImprovementRegistry : public Registry<ImprovementConfig_t, ImprovementConfigParser>
{
private:
    // Hidden, so improvements.json cannot be loaded without terrain; LoadMapOccupants fills it.
    using Registry<ImprovementConfig_t, ImprovementConfigParser>::Load;
};

} // namespace ac
