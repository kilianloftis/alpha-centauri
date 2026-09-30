#include "game/ecology/EcoDamageCalculator.h"

#include "game/ecology/EcoDamageConfig.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

EcoDamageCalculator::EcoDamageCalculator(const EcoDamageConfig_t& rConfig, LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
{
}

int EcoDamageCalculator::Calculate(const EcoDamageInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"terraform_raw",        rInputs.terraformRaw},
        {"terraform_scale",      rInputs.terraformScale},
        {"minerals",             static_cast<double>(rInputs.minerals)},
        {"mineral_offset",       static_cast<double>(rInputs.mineralOffset)},
        {"clean_minerals",       static_cast<double>(rInputs.cleanMinerals)},
        {"fungal_blooms",        static_cast<double>(rInputs.fungalBlooms)},
        {"clean_mineral_grants", static_cast<double>(rInputs.cleanMineralGrants)},
        {"virtual_minerals",     static_cast<double>(rInputs.virtualMinerals)},
        {"damage_reduction",     static_cast<double>(rInputs.damageReduction)},
        {"techs",                static_cast<double>(rInputs.techs)},
        {"eco_scale",            rInputs.ecoScale},
    };
    const int damage = m_rLua.EvalInt(m_rConfig.damageFormula, vars);
    if (damage < 0)
    {
        throw std::runtime_error("Eco damage formula produced " + std::to_string(damage)
                                 + "; the fungal-pop chance cannot be negative");
    }
    return damage;
}

} // namespace ac
