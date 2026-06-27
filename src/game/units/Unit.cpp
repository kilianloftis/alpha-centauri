#include "game/units/Unit.h"

namespace ac
{

Unit::Unit(const std::string& rName,
           const ChassisConfig_t& rChassis,
           const WeaponConfig_t& rWeapon,
           const ArmourConfig_t& rArmour,
           const ReactorConfig_t& rReactor,
           const AbilityConfig_t* pAbility1,
           const AbilityConfig_t* pAbility2)
    : m_name(rName)
    , m_rChassis(rChassis)
    , m_rWeapon(rWeapon)
    , m_rArmour(rArmour)
    , m_rReactor(rReactor)
    , m_pAbility1(pAbility1)
    , m_pAbility2(pAbility2)
{
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
    int cost = m_rChassis.mineralCost
             + m_rWeapon.mineralCost
             + m_rArmour.mineralCost
             + m_rReactor.mineralCost;

    if (m_pAbility1)
    {
        cost += m_pAbility1->mineralCost;
    }
    if (m_pAbility2)
    {
        cost += m_pAbility2->mineralCost;
    }

    return cost;
}

} // namespace ac
