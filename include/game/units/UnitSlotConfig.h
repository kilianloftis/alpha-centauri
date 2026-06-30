#pragma once

#include <string>

namespace ac
{

struct UnitSlotConfig_t
{
    std::string id;
    std::string displayName;
    std::string componentType; // must match UnitComponentConfig_t::type value in JSON
    bool required = true;
    std::string column;        // "left" or "right"
    float costModifier = 1.0f; // scales this slot's component mineral cost
    std::string requiredTech;  // slot is only available once this tech is discovered
};

} // namespace ac
