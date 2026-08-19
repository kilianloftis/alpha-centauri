#pragma once

#include "game/population/pop-types/PopCompositionConfigParser.h"
#include <string>
#include <unordered_map>

namespace ac
{

class LuaRuntime;

struct PopCompositionInputs_t
{
    int targetDrones = 0;
    int psychOutput = 0;
    int factionTalentModifier = 0;
};

struct PopCompositionResult_t
{
    int targetDrones = 0;
    int targetTalents = 0;
};

// Combines a precomputed drone target with the talent formula from pop_composition.json.
// Talent formula variables: psych_output, faction_talent_modifier
class PopCompositionCalculator
{
public:
    PopCompositionCalculator(const PopCompositionConfig_t& rConfig, LuaRuntime& rLua);
    ~PopCompositionCalculator() = default;

    PopCompositionResult_t Calculate(const PopCompositionInputs_t& rInputs);
    const PopCompositionConfig_t& GetConfig() const;

private:
    const PopCompositionConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

} // namespace ac
