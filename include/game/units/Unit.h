#pragma once

#include "game/IConstructable.h"
#include "game/units/chassis/ChassisConfigParser.h"
#include "game/units/weapon/WeaponConfigParser.h"
#include "game/units/armour/ArmourConfigParser.h"
#include "game/units/reactor/ReactorConfigParser.h"
#include "game/units/ability/AbilityConfigParser.h"
#include <string>

namespace ac
{

class Unit : public IConstructable
{
public:
    Unit(const std::string& rName,
         const ChassisConfig_t& rChassis,
         const WeaponConfig_t& rWeapon,
         const ArmourConfig_t& rArmour,
         const ReactorConfig_t& rReactor,
         const AbilityConfig_t* pAbility1 = nullptr,
         const AbilityConfig_t* pAbility2 = nullptr);
    ~Unit() = default;

    const char* GetId() const override;
    const std::string& GetName() const override;
    int GetBaseCost() const override;

private:
    std::string m_name;
    const ChassisConfig_t& m_rChassis;
    const WeaponConfig_t& m_rWeapon;
    const ArmourConfig_t& m_rArmour;
    const ReactorConfig_t& m_rReactor;
    const AbilityConfig_t* m_pAbility1;
    const AbilityConfig_t* m_pAbility2;
};

} // namespace ac
