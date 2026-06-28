#pragma once

namespace ac
{

struct UnitComponentConfig_t;

struct UnitDesignerState_t
{
    const UnitComponentConfig_t* pChassis = nullptr;
    const UnitComponentConfig_t* pWeapon  = nullptr;
    const UnitComponentConfig_t* pArmour  = nullptr;
    const UnitComponentConfig_t* pReactor = nullptr;
    const UnitComponentConfig_t* pAbility1 = nullptr;
    const UnitComponentConfig_t* pAbility2 = nullptr;

    bool HasAllMandatory() const
    {
        return pChassis && pWeapon && pArmour && pReactor;
    }
};

} // namespace ac
