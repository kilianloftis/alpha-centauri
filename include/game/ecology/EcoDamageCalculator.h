#pragma once

namespace ac
{

class LuaRuntime;
struct EcoDamageConfig_t;

// Every runtime input config/eco_damage.lua reads. Assembled by BaseEcology.
struct EcoDamageInputs_t
{
    // Σ EcoDamageContribution over the base radius, plus EcoDamageWorkedContribution on the
    // tiles this base's own pops work.
    double terraformRaw = 0.0;
    double terraformScale = 1.0;
    int minerals = 0;
    int mineralOffset = 0;
    // The resolved EcoCleanMinerals baseline; blooms and grants are separate inputs.
    int cleanMinerals = 0;
    int fungalBlooms = 0;
    int cleanMineralGrants = 0;
    // Atrocity records plus EcologyLedger virtual minerals, already weighted.
    int virtualMinerals = 0;
    int damageReduction = 0;
    int techs = 0;
    // The resolved EcologicalDamage multiplier stack.
    double ecoScale = 1.0;
};

class EcoDamageCalculator
{
public:
    EcoDamageCalculator(const EcoDamageConfig_t& rConfig, LuaRuntime& rLua);

    // The fungal-pop percentage. Throws if the formula fails or yields a negative value.
    int Calculate(const EcoDamageInputs_t& rInputs) const;

private:
    const EcoDamageConfig_t& m_rConfig;
    LuaRuntime& m_rLua;
};

} // namespace ac
