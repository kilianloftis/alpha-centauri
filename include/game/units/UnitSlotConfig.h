#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace ac
{

// Which designer column a slot is laid out in. A string here meant every value that was not
// "right" — including a typo — silently became left.
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

    // Empty requiredTech = always available (matches BuildingConfig_t::IsAvailable).
    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechs) const
    {
        if (requiredTech.empty())
        {
            return true;
        }
        return std::find(rDiscoveredTechs.begin(), rDiscoveredTechs.end(), requiredTech)
               != rDiscoveredTechs.end();
    }
};

} // namespace ac
