#pragma once

#include "game/production/StockpileConfig.h"
#include <nlohmann/json.hpp>
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

private:
    StockpileConfig_t ParseStockpileConfig_(const nlohmann::json& stockpileJson);
};

} // namespace ac
