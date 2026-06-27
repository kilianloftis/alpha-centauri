#include "game/units/Unit.h"
#include <stdexcept>

namespace ac
{

Unit::Unit(const std::string& rName,
           const UnitComponentConfig_t& rChassis,
           const UnitComponentConfig_t& rWeapon,
           const UnitComponentConfig_t& rArmour,
           const UnitComponentConfig_t& rReactor,
           const UnitComponentConfig_t* pAbility1,
           const UnitComponentConfig_t* pAbility2)
    : m_name(rName)
    , m_rChassis(rChassis)
    , m_rWeapon(rWeapon)
    , m_rArmour(rArmour)
    , m_rReactor(rReactor)
    , m_pAbility1(pAbility1)
    , m_pAbility2(pAbility2)
{
    if (m_rChassis.type != UnitComponentType_t::Chassis)
        throw std::runtime_error("Expected chassis component for chassis slot");
    if (m_rWeapon.type != UnitComponentType_t::Weapon)
        throw std::runtime_error("Expected weapon component for weapon slot");
    if (m_rArmour.type != UnitComponentType_t::Armour)
        throw std::runtime_error("Expected armour component for armour slot");
    if (m_rReactor.type != UnitComponentType_t::Reactor)
        throw std::runtime_error("Expected reactor component for reactor slot");
    if (m_pAbility1 && m_pAbility1->type != UnitComponentType_t::Ability)
        throw std::runtime_error("Expected ability component for ability slot");
    if (m_pAbility2 && m_pAbility2->type != UnitComponentType_t::Ability)
        throw std::runtime_error("Expected ability component for ability slot");
}

const char* Unit::GetId() const
{
    return m_name.c_str();
}

const std::string& Unit::GetName() const
{
    return m_name;
}

int Unit::GetBaseCost() const
{
    int rawCost = m_rChassis.mineralCost + m_rWeapon.mineralCost
                + m_rArmour.mineralCost + m_rReactor.mineralCost;
    if (m_pAbility1) rawCost += m_pAbility1->mineralCost;
    if (m_pAbility2) rawCost += m_pAbility2->mineralCost;

    // cost_multiplier uses only the geometric chain across all components
    float costMult = 1.0f;
    auto applyCostMult = [&](const UnitComponentConfig_t& rComp)
    {
        auto it = rComp.stats.find("cost_multiplier");
        if (it != rComp.stats.end())
            costMult *= it->second.geometricMult;
    };
    applyCostMult(m_rChassis);
    applyCostMult(m_rWeapon);
    applyCostMult(m_rArmour);
    applyCostMult(m_rReactor);
    if (m_pAbility1) applyCostMult(*m_pAbility1);
    if (m_pAbility2) applyCostMult(*m_pAbility2);

    return static_cast<int>(rawCost * costMult);
}

float Unit::ResolveStat_(const std::string& rStatName) const
{
    float base = 0.0f;
    float additive = 0.0f;
    float geometric = 1.0f;

    auto accumulate = [&](const UnitComponentConfig_t& rComp)
    {
        auto it = rComp.stats.find(rStatName);
        if (it != rComp.stats.end())
        {
            base += it->second.base;
            additive += it->second.additiveMult;
            geometric *= it->second.geometricMult;
        }
    };

    accumulate(m_rChassis);
    accumulate(m_rWeapon);
    accumulate(m_rArmour);
    accumulate(m_rReactor);
    if (m_pAbility1) accumulate(*m_pAbility1);
    if (m_pAbility2) accumulate(*m_pAbility2);

    return base * (1.0f + additive) * geometric;
}

bool Unit::ResolveFlag_(const std::string& rFlagName) const
{
    auto check = [&](const UnitComponentConfig_t& rComp)
    {
        auto it = rComp.flags.find(rFlagName);
        return it != rComp.flags.end() && it->second;
    };

    return check(m_rChassis) || check(m_rWeapon) || check(m_rArmour) || check(m_rReactor)
        || (m_pAbility1 != nullptr && check(*m_pAbility1))
        || (m_pAbility2 != nullptr && check(*m_pAbility2));
}

std::unordered_map<std::string, float> Unit::ResolveBonusTable_(const std::string& rTableName) const
{
    std::unordered_map<std::string, float> result;

    auto accumulate = [&](const UnitComponentConfig_t& rComp)
    {
        auto tableIt = rComp.bonusTables.find(rTableName);
        if (tableIt != rComp.bonusTables.end())
        {
            for (const auto& [key, val] : tableIt->second)
                result[key] += val;
        }
    };

    accumulate(m_rChassis);
    accumulate(m_rWeapon);
    accumulate(m_rArmour);
    accumulate(m_rReactor);
    if (m_pAbility1) accumulate(*m_pAbility1);
    if (m_pAbility2) accumulate(*m_pAbility2);

    return result;
}

int Unit::GetAttack() const               { return static_cast<int>(ResolveStat_("attack")); }
int Unit::GetDefense() const              { return static_cast<int>(ResolveStat_("defense")); }
int Unit::GetMovement() const             { return static_cast<int>(ResolveStat_("movement")); }
int Unit::GetHitPoints() const            { return static_cast<int>(ResolveStat_("hit_points")); }
int Unit::GetDisengageChance() const      { return static_cast<int>(ResolveStat_("disengage_chance")); }
int Unit::GetFuel() const                 { return static_cast<int>(ResolveStat_("fuel")); }
int Unit::GetDamageFromOutOfFuel() const  { return static_cast<int>(ResolveStat_("damage_from_out_of_fuel")); }
bool Unit::IsFlight() const               { return ResolveFlag_("flight"); }
int Unit::GetCargoCapacity() const        { return static_cast<int>(ResolveStat_("cargo_capacity")); }
int Unit::GetDifficultTerrainCost() const { return static_cast<int>(ResolveStat_("difficult_terrain_cost")); }
bool Unit::IsSingleUse() const            { return ResolveFlag_("single_use"); }

std::unordered_map<std::string, float> Unit::GetTerrainAttackBonus() const
{
    return ResolveBonusTable_("terrain_attack");
}

} // namespace ac
