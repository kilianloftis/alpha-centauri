#pragma once

#include "game/IConstructable.h"
#include "game/units/UnitComponentConfig.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

class UnitDesign : public IConstructable
{
public:
    UnitDesign(const UnitComponentConfig_t& rChassis,
         const UnitComponentConfig_t& rWeapon,
         const UnitComponentConfig_t& rArmour,
         const UnitComponentConfig_t& rReactor,
         const UnitComponentConfig_t* pAbility1 = nullptr,
         const UnitComponentConfig_t* pAbility2 = nullptr);
    ~UnitDesign() = default;

    const char* GetId() const override;
    const std::string& GetName() const override;
    int GetBaseCost() const override;

    const UnitComponentConfig_t& GetChassis() const;
    const UnitComponentConfig_t& GetWeapon() const;
    const UnitComponentConfig_t& GetArmour() const;
    const UnitComponentConfig_t& GetReactor() const;
    const UnitComponentConfig_t* GetAbility1() const;
    const UnitComponentConfig_t* GetAbility2() const;

    int GetAttack() const;
    int GetDefense() const;
    int GetMovement() const;
    int GetHitPoints() const;
    int GetDisengageChance() const;
    int GetFuel() const;
    int GetDamageFromOutOfFuel() const;
    bool IsFlight() const;
    int GetCargoCapacity() const;
    int GetDifficultTerrainCost() const;
    bool IsSingleUse() const;
    std::unordered_map<std::string, float> GetTerrainAttackBonus() const;

private:
    float ResolveStat_(const std::string& rStatName) const;
    bool ResolveFlag_(const std::string& rFlagName) const;
    std::unordered_map<std::string, float> ResolveBonusTable_(const std::string& rTableName) const;

    std::string m_id;
    std::string m_name;
    const UnitComponentConfig_t* m_pChassis;
    const UnitComponentConfig_t* m_pWeapon;
    const UnitComponentConfig_t* m_pArmour;
    const UnitComponentConfig_t* m_pReactor;
    const UnitComponentConfig_t* m_pAbility1;
    const UnitComponentConfig_t* m_pAbility2;
    std::vector<const UnitComponentConfig_t*> m_components;
};

} // namespace ac
