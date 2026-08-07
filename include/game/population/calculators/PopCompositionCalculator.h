#pragma once

#include "game/population/pop-types/PopCompositionConfigParser.h"
#include <string>
#include <unordered_map>

namespace ac
{

class LuaRuntime;

// Input variables available to composition formulas.
struct PopCompositionInputs_t
{
    int baseSize            = 0;
    int psychOutput         = 0;
    int factionDroneModifier  = 0;
    int factionTalentModifier = 0;
};

// Result of a composition calculation: target counts per named type.
struct PopCompositionResult_t
{
    int targetDrones  = 0;
    int targetTalents = 0;
};

// Evaluates pop composition formulas from a PopCompositionConfig_t via Lua.
// Formula variables available: base_size, psych_output,
//                              faction_drone_modifier, faction_talent_modifier
// Formulas are standard Lua expressions (math library available).
class PopCompositionCalculator
{
public:
    PopCompositionCalculator(const PopCompositionConfig_t& rConfig, LuaRuntime& rLua);
    ~PopCompositionCalculator() = default;

    // Calculate target drone and talent counts given runtime inputs.
    // Throws if a formula fails or produces a negative target.
    PopCompositionResult_t Calculate(const PopCompositionInputs_t& rInputs);

    // Access the underlying config (drone/talent type ids, formulas, etc.)
    const PopCompositionConfig_t& GetConfig() const;

private:
    const PopCompositionConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

} // namespace ac
