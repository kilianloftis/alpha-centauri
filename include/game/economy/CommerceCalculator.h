#pragma once

namespace ac
{

struct CommerceConfig_t;
class LuaRuntime;

// The formula variables commerce.json's pair formula reads, besides its pair_multiplier.
struct CommercePairInputs_t
{
    int energyOurs = 0;
    int energyTheirs = 0;
    int commerceTech = 0;
    int techDenominator = 1;
};

// Evaluates commerce.json's pair income formula. Pairing, sanctions and the CommerceRate and
// CommerceEnergyBonus effects are CommerceRules' job.
class CommerceCalculator
{
public:
    CommerceCalculator(const CommerceConfig_t& rConfig, LuaRuntime& rLua);

    int PairValue(const CommercePairInputs_t& rInputs) const;

private:
    const CommerceConfig_t& m_rConfig;
    // Not const: EvalInt mutates interpreter globals.
    LuaRuntime& m_rLua;
};

} // namespace ac
