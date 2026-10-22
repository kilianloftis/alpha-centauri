#pragma once

#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"

#include <string>
#include <vector>

namespace ac
{

class UnitComponentRegistry;
class UnitSlotRegistry;

// Unit-design options the discovered techs allow.

std::vector<UnitSlotConfig_t> GetAvailableUnitSlots(
    const UnitSlotRegistry& rSlots,
    const std::vector<std::string>& rDiscoveredTechs);

std::vector<const UnitComponentConfig_t*> GetAvailableUnitComponents(
    const UnitComponentRegistry& rComponents,
    const std::vector<std::string>& rDiscoveredTechs);

std::vector<const UnitComponentConfig_t*> GetAvailableUnitComponents(
    const UnitComponentRegistry& rComponents,
    const std::string& rComponentType,
    const std::vector<std::string>& rDiscoveredTechs);

} // namespace ac
