#pragma once

#include "lib/Formula.h"

#include <string>

namespace ac
{

class LuaRuntime;

// All runtime inputs required to calculate the research cost for a tech.
struct TechCostInputs_t
{
    int techs = 0;                   // max(1, discovered count)
    int mostTechs = 0;               // max discovered count across every session faction
    // From StatId_t::TechCostDiff (difficulty Add). Shipping: 1=Citizen … 5=Transcend.
    int diff = 1;
    // From StatId_t::TechCostCatchUp (difficulty Add). Shipping: 0=Citizen … 5=Transcend.
    int catchUpDiff = 0;
    int turns = 0;                   // GameState::GetTurnNumber (terranx CurrentTurn)
    bool bIsAI = false;
    bool bTurnLag = false;           // TechCostTurnLag (nonzero is on)
    // From StatId_t::TechCostResearch (inherent SOCIAL RESEARCH Add).
    int research = 0;
    int mapAreaSqRoot = 56;          // WorldMap::GetAreaSqRoot; 56 is identity vs the divisor
};

struct TechCostConfig_t
{
    Formula_t costFormula;
};

class TechCostConfigParser
{
public:
    static TechCostConfig_t ParseConfig(const std::string& scriptPath, LuaRuntime& rLua);
};

} // namespace ac
