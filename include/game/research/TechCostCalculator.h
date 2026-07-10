#pragma once

#include "game/research/TechConfigParser.h"
#include "game/research/TechCostConfig.h"

namespace ac
{

class LuaRuntime;

class TechCostCalculator
{
public:
    TechCostCalculator(const TechCostConfig_t& rConfig, LuaRuntime& rLua);
    ~TechCostCalculator();

    int CalculateCost(const TechConfig_t& rTech, const TechCostInputs_t& rInputs) const;

private:
    const TechCostConfig_t* m_pConfig;
    LuaRuntime* m_pLua;
};
}

