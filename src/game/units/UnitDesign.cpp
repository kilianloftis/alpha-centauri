#include "game/units/UnitDesign.h"
#include <stdexcept>

namespace ac
{

UnitDesign::UnitDesign(const UnitComponentConfig_t& rChassis,
           const UnitComponentConfig_t& rWeapon,
           const UnitComponentConfig_t& rArmour,
           const UnitComponentConfig_t& rReactor,
           const UnitComponentConfig_t* pAbility1,
           const UnitComponentConfig_t* pAbility2)
{
    if (rChassis.type != UnitComponentType_t::Chassis)
        throw std::runtime_error("Expected chassis component for chassis slot");
    if (rWeapon.type != UnitComponentType_t::Weapon)
        throw std::runtime_error("Expected weapon component for weapon slot");
    if (rArmour.type != UnitComponentType_t::Armour)
        throw std::runtime_error("Expected armour component for armour slot");
    if (rReactor.type != UnitComponentType_t::Reactor)
        throw std::runtime_error("Expected reactor component for reactor slot");
    if (pAbility1 && pAbility1->type != UnitComponentType_t::Ability)
        throw std::runtime_error("Expected ability component for ability slot");
    if (pAbility2 && pAbility2->type != UnitComponentType_t::Ability)
        throw std::runtime_error("Expected ability component for ability slot");

    m_pChassis  = &rChassis;
    m_pWeapon   = &rWeapon;
    m_pArmour   = &rArmour;
    m_pReactor  = &rReactor;
    m_pAbility1 = pAbility1;
    m_pAbility2 = pAbility2;

    m_components = { &rChassis, &rWeapon, &rArmour, &rReactor };
    if (pAbility1) m_components.push_back(pAbility1);
    if (pAbility2) m_components.push_back(pAbility2);

    m_name = rChassis.name + " / " + rWeapon.name + " / " + rArmour.name + " / " + rReactor.name;
    m_id   = rChassis.id + "_" + rWeapon.id + "_" + rArmour.id + "_" + rReactor.id;
}

const char* UnitDesign::GetId() const          { return m_id.c_str(); }
const std::string& UnitDesign::GetName() const { return m_name; }

const UnitComponentConfig_t& UnitDesign::GetChassis()  const { return *m_pChassis; }
const UnitComponentConfig_t& UnitDesign::GetWeapon()   const { return *m_pWeapon; }
const UnitComponentConfig_t& UnitDesign::GetArmour()   const { return *m_pArmour; }
const UnitComponentConfig_t& UnitDesign::GetReactor()  const { return *m_pReactor; }
const UnitComponentConfig_t* UnitDesign::GetAbility1() const { return m_pAbility1; }
const UnitComponentConfig_t* UnitDesign::GetAbility2() const { return m_pAbility2; }

int UnitDesign::GetBaseCost() const
{
    int rawCost = 0;
    float costMult = 1.0f;

    for (const UnitComponentConfig_t* pComp : m_components)
    {
        rawCost += pComp->mineralCost;

        auto it = pComp->stats.find("cost_multiplier");
        if (it != pComp->stats.end())
            costMult *= it->second.geometricMult;
    }

    return static_cast<int>(rawCost * costMult);
}

float UnitDesign::ResolveStat_(const std::string& rStatName) const
{
    float base = 0.0f;
    float additive = 0.0f;
    float geometric = 1.0f;

    for (const UnitComponentConfig_t* pComp : m_components)
    {
        auto it = pComp->stats.find(rStatName);
        if (it != pComp->stats.end())
        {
            base += it->second.base;
            additive += it->second.additiveMult;
            geometric *= it->second.geometricMult;
        }
    }

    return base * (1.0f + additive) * geometric;
}

bool UnitDesign::ResolveFlag_(const std::string& rFlagName) const
{
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        auto it = pComp->flags.find(rFlagName);
        if (it != pComp->flags.end() && it->second)
            return true;
    }
    return false;
}

std::unordered_map<std::string, float> UnitDesign::ResolveBonusTable_(const std::string& rTableName) const
{
    std::unordered_map<std::string, float> result;

    for (const UnitComponentConfig_t* pComp : m_components)
    {
        auto tableIt = pComp->bonusTables.find(rTableName);
        if (tableIt != pComp->bonusTables.end())
        {
            for (const auto& [key, val] : tableIt->second)
                result[key] += val;
        }
    }

    return result;
}

int UnitDesign::GetAttack() const               { return static_cast<int>(ResolveStat_("attack")); }
int UnitDesign::GetDefense() const              { return static_cast<int>(ResolveStat_("defense")); }
int UnitDesign::GetMovement() const             { return static_cast<int>(ResolveStat_("movement")); }
int UnitDesign::GetHitPoints() const            { return static_cast<int>(ResolveStat_("hit_points")); }
int UnitDesign::GetDisengageChance() const      { return static_cast<int>(ResolveStat_("disengage_chance")); }
int UnitDesign::GetFuel() const                 { return static_cast<int>(ResolveStat_("fuel")); }
int UnitDesign::GetDamageFromOutOfFuel() const  { return static_cast<int>(ResolveStat_("damage_from_out_of_fuel")); }
bool UnitDesign::IsFlight() const               { return ResolveFlag_("flight"); }
int UnitDesign::GetCargoCapacity() const        { return static_cast<int>(ResolveStat_("cargo_capacity")); }
int UnitDesign::GetDifficultTerrainCost() const { return static_cast<int>(ResolveStat_("difficult_terrain_cost")); }
bool UnitDesign::IsSingleUse() const            { return ResolveFlag_("single_use"); }

std::unordered_map<std::string, float> UnitDesign::GetTerrainAttackBonus() const
{
    return ResolveBonusTable_("terrain_attack");
}

} // namespace ac
