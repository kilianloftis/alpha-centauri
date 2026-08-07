#pragma once

#include "game/buildings/BuildingConfigParser.h"
#include "lib/Registry.h"

#include <stdexcept>

namespace ac
{

class BuildingRegistry : public Registry<BuildingConfig_t, BuildingConfigParser>
{
protected:
    // The whole-set checks the base class exists to host. BuildingRegistry inherited the
    // extension point and never used it, so cross-entry mistakes that only make sense to catch
    // once every building is loaded went unnoticed until they mattered at runtime.
    void Validate_() override
    {
        Registry::Validate_();

        for (const BuildingConfig_t& rConfig : this->m_configs)
        {
            // A secret project is by definition unique in the world, so stacking copies of one
            // is a contradiction the availability rules cannot express. Caught here rather than
            // as a confusing "already built" throw the first time one is completed.
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
        }
    }
};

} // namespace ac
