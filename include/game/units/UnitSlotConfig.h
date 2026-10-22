#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace ac
{

enum class SlotColumn_t
{
    Left,
    Right
};

struct UnitSlotConfig_t
{
    std::string id;
    std::string displayName;
    std::string componentType; // must match UnitComponentConfig_t::type value in JSON
    bool required = true;
    SlotColumn_t column = SlotColumn_t::Left;
    float costModifier = 1.0f; // scales this slot's component mineral cost
    std::string requiredTech;  // slot is only available once this tech is discovered

    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechs) const
    {
        return requiredTech.empty() || std::ranges::contains(rDiscoveredTechs, requiredTech);
    }
};

} // namespace ac
