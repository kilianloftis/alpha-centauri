#pragma once

#include "game/buildings/BuildingConfigParser.h"
#include "lib/Registry.h"

#include <stdexcept>

namespace ac
{

class BuildingRegistry : public Registry<BuildingConfig_t, BuildingConfigParser>
{
protected:
    // Cross-entry checks that only make sense once every building is loaded.
    void Validate_() override
    {
        Registry::Validate_();

        for (const BuildingConfig_t& rConfig : this->m_configs)
        {
            // A secret project is unique in the world, so allow_multiple is unexpressible.
            if (rConfig.bIsSecretProject && rConfig.allowMultiple)
            {
                throw std::runtime_error(
                    "Building '" + rConfig.id
                    + "': secret_project and allow_multiple are mutually exclusive");
            }
            if (rConfig.mineralCost < 0)
            {
                throw std::runtime_error("Building '" + rConfig.id
                                         + "': mineral_cost must not be negative");
            }
            if (rConfig.upkeep < 0)
            {
                throw std::runtime_error("Building '" + rConfig.id
                                         + "': upkeep must not be negative");
            }
        }
    }
};

} // namespace ac
