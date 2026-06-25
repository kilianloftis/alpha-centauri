#include "game/research/TechRegistry.h"
#include <algorithm>
#include <iostream>

namespace ac
{

void TechRegistry::Validate_()
{
    Registry::Validate_();

    const auto& configs = GetAll();
    for (const TechConfig_t& rConfig : configs)
    {
        ValidatePrerequisites_(rConfig, configs);
    }

    std::cout << "Loaded and validated " << configs.size() << " tech configurations\n";
}

void TechRegistry::ValidatePrerequisites_(const TechConfig_t& config, const std::vector<TechConfig_t>& configs)
{
    // Check for self-reference
    if (std::find(config.prerequisites.begin(), config.prerequisites.end(), config.id) != config.prerequisites.end())
    {
        throw std::runtime_error("Tech '" + config.id + "' cannot have itself as a prerequisite");
    }

    // Check all prerequisites exist
    for (const std::string& prereqId : config.prerequisites)
    {
        auto it = std::find_if(configs.begin(), configs.end(),
            [&prereqId](const TechConfig_t& c) { return c.id == prereqId; });
        if (it == configs.end())
        {
            throw std::runtime_error("Prerequisite '" + prereqId + "' not found for tech '" + config.id + "'");
        }
    }
}


} // namespace ac
