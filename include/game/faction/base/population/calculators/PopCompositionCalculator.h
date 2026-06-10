#pragma once

#include "game/faction/base/population/pop-types/PopCompositionConfigParser.h"
#include <string>
#include <unordered_map>

namespace ac
{

class LuaRuntime;

// Input variables available to composition formulas.
struct PopCompositionInputs
{
    int baseSize            = 0;
    int psychOutput         = 0;
    int factionDroneModifier  = 0;
    int factionTalentModifier = 0;
};

// Result of a composition calculation: target counts per named type.
struct PopCompositionResult
{
    int targetDrones  = 0;
    int targetTalents = 0;
};

// Evaluates pop composition formulas from a PopCompositionConfig via Lua.
// Formula variables available: base_size, psych_output,
//                              faction_drone_modifier, faction_talent_modifier
// Formulas are standard Lua expressions (math library available).
class PopCompositionCalculator
{
public:
    PopCompositionCalculator(const PopCompositionConfig& rConfig, LuaRuntime& rLua);
    ~PopCompositionCalculator() = default;

    // Calculate target drone and talent counts given runtime inputs.
    PopCompositionResult Calculate(const PopCompositionInputs& inputs);

    // Access the underlying config (e.g. to read defaultType)
    const PopCompositionConfig& GetConfig() const;

private:
    const PopCompositionConfig* m_pConfig;
    LuaRuntime* m_pLua;
};

} // namespace ac
