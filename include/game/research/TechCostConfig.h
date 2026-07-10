#pragma once

#include <string>

namespace ac
{

class LuaRuntime;

// All runtime inputs required to calculate the research cost for a tech.
struct TechCostInputs_t
{
    int techs = 0;                   // TECHS: discovered techs (excl. starting) + varA - varB
    int mostTechs = 0;               // MOSTTECHS: max techs by any faction + varA
    int diff = 1;                    // 1=Citizen, 2=Specialist, 3=Talent/Librarian, 4=Thinker, 5=Transcend
    int turns = 0;                   // Number of turns elapsed
    bool bIsAI = false;
    bool bTechStagnation = false;
    int researchModifier = 0;        // -1=natural bonus, 0=neutral, +1=natural penalty
    int worldSizeModifier = 0;       // Percentage modifier from world size (0 = no change)
    int factionTechCostModifier = 0; // Percentage modifier from TechCost bonus effects
    int alphaxTechCostModifier = 0;  // Percentage modifier from alpha(x).txt
};

struct TechCostConfig_t
{
    std::string costFormula;
};

class TechCostConfigParser
{
public:
    TechCostConfigParser() = default;
    ~TechCostConfigParser() = default;

    TechCostConfig_t ParseConfig(const std::string& scriptPath, LuaRuntime& rLua);
};

} // namespace ac
