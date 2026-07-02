#pragma once

#include <string>

namespace ac
{

struct GrowthConfig_t
{
    int nutrientsPerPop = 10;  // nutrients required per current population size to grow
};

class GrowthConfig_tParser
{
public:
    GrowthConfig_tParser() = default;
    ~GrowthConfig_tParser() = default;

    // Load pop_growth.json.
    // Returns a default config on failure.
    GrowthConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
