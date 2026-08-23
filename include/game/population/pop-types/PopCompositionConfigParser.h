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
    // Peak extra drones on the capture turn, decaying by 1 every assimilationDecayTurns.
    // Fresh-capture duration is the product (shipping 5 × 10 = 50).
    int assimilationDrones = 0;
    int assimilationDecayTurns = 0;
};

class PopCompositionConfigParser
{
public:
    PopCompositionConfigParser() = default;
    ~PopCompositionConfigParser() = default;

    PopCompositionConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
