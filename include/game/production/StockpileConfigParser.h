#pragma once

#include "game/production/StockpileConfig.h"
#include <string>
#include <vector>

namespace ac
{

class StockpileConfigParser
{
public:
    StockpileConfigParser() = default;
    ~StockpileConfigParser() = default;

    std::vector<StockpileConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
