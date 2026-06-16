#pragma once

#include "game/research/Tech.h"
#include "game/research/TechCostConfig.h"

namespace ac
{

class LuaRuntime;

class TechCostCalculator
{
public:
    TechCostCalculator(const TechCostConfig& rConfig, LuaRuntime& rLua);
    ~TechCostCalculator();

    int CalculateCost(const Tech& rTech, const TechCostInputs_t& rInputs) const;

private:
    const TechCostConfig* m_pConfig;
    LuaRuntime* m_pLua;
};
}

