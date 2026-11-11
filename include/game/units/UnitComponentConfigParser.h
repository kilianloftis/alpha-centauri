#pragma once

#include "game/units/UnitComponentConfig.h"
#include <string>
#include <vector>

namespace ac
{

class UnitComponentRegistry;

// Throws if any requires_chassis id is missing or is not a chassis component.
void ValidateComponentChassisRequirements(const UnitComponentRegistry& rRegistry);

class UnitComponentConfigParser
{
public:
    std::vector<UnitComponentConfig_t> ParseConfig(const std::string& rConfigPath);
};

} // namespace ac
