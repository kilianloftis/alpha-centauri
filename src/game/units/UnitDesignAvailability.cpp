#include "game/units/UnitDesignAvailability.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"

namespace ac
{

std::vector<UnitSlotConfig_t> GetAvailableUnitSlots(
    const UnitSlotRegistry& rSlots,
    const std::vector<std::string>& rDiscoveredTechs)
{
    std::vector<UnitSlotConfig_t> available;
    for (const UnitSlotConfig_t& rSlot : rSlots.GetAll())
    {
        if (rSlot.IsAvailable(rDiscoveredTechs))
        {
            available.push_back(rSlot);
        }
    }
    return available;
}

std::vector<const UnitComponentConfig_t*> GetAvailableUnitComponents(
    const UnitComponentRegistry& rComponents,
    const std::vector<std::string>& rDiscoveredTechs)
{
    std::vector<const UnitComponentConfig_t*> available;
    for (const UnitComponentConfig_t& rConfig : rComponents.GetAll())
    {
        if (rConfig.IsAvailable(rDiscoveredTechs))
        {
            available.push_back(&rConfig);
        }
    }
    return available;
}

std::vector<const UnitComponentConfig_t*> GetAvailableUnitComponents(
    const UnitComponentRegistry& rComponents,
    const std::string& rComponentType,
    const std::vector<std::string>& rDiscoveredTechs)
{
    std::vector<const UnitComponentConfig_t*> available;
    for (const UnitComponentConfig_t& rConfig : rComponents.GetAll())
    {
        if (rConfig.type == rComponentType && rConfig.IsAvailable(rDiscoveredTechs))
        {
            available.push_back(&rConfig);
        }
    }
    return available;
}

} // namespace ac
