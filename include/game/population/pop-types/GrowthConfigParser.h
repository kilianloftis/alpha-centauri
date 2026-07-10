#pragma once

#include <string>

namespace ac
{

struct GrowthConfig_t
{
    int nutrientsPerPop = 10;  // nutrients required per current population size to grow
    int maxBaseSize = 7;       // SMAC population limit without a Hab Complex
};

class GrowthConfigParser
{
public:
    GrowthConfigParser() = default;
    ~GrowthConfigParser() = default;

    // Load pop_growth.json. Throws if the file cannot be opened or parsed.
    GrowthConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
