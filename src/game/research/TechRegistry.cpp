#include "game/research/TechRegistry.h"

#include "lib/CycleCheck.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

void TechRegistry::Validate_()
{
    Registry::Validate_();

    const auto& rConfigs = GetAll();
    for (const TechConfig_t& rConfig : rConfigs)
    {
        ValidatePrerequisites_(rConfig, rConfigs);
    }
    ValidateNoPrerequisiteCycles_();

    std::cout << "Loaded and validated " << rConfigs.size() << " tech configurations\n";
}

void TechRegistry::ValidatePrerequisites_(const TechConfig_t& rConfig,
                                          const std::vector<TechConfig_t>& rConfigs)
{
    if (std::find(rConfig.prerequisites.begin(), rConfig.prerequisites.end(), rConfig.id)
        != rConfig.prerequisites.end())
    {
        throw std::runtime_error("Tech '" + rConfig.id + "' cannot have itself as a prerequisite");
    }

    for (const std::string& rPrereqId : rConfig.prerequisites)
    {
        const auto it = std::find_if(rConfigs.begin(), rConfigs.end(),
            [&rPrereqId](const TechConfig_t& rOther) { return rOther.id == rPrereqId; });
        if (it == rConfigs.end())
        {
            throw std::runtime_error("Prerequisite '" + rPrereqId + "' not found for tech '"
                                     + rConfig.id + "'");
        }
    }
}

// A cycle can never be researched, so it is a broken tree, reported at load.
void TechRegistry::ValidateNoPrerequisiteCycles_() const
{
    RequireAcyclic(
        GetAll(),
        [this](const std::string& rId) -> const std::vector<std::string>& {
            return Get(rId).prerequisites;
        },
        "Tech prerequisite cycle");
}

} // namespace ac
