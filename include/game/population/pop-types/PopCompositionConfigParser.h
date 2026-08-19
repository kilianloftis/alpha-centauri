#pragma once

#include <string>

namespace ac
{

struct PopCompositionConfig_t
{
    std::string bureaucracyLimitFormula;
    std::string droneFormula;
    std::string talentFormula;
    std::string droneTypeId;
    std::string talentTypeId;
};

class PopCompositionConfigParser
{
public:
    PopCompositionConfigParser() = default;
    ~PopCompositionConfigParser() = default;

    PopCompositionConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
