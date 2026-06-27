#pragma once

#include "game/IConstructable.h"
#include "game/units/UnitComponentConfig.h"
#include <string>
#include <unordered_map>

namespace ac
{

class Unit : public IConstructable
{
public:
    Unit(const std::string& rName,
         const UnitComponentConfig_t& rChassis,
         const UnitComponentConfig_t& rWeapon,
         const UnitComponentConfig_t& rArmour,
         const UnitComponentConfig_t& rReactor,
         const UnitComponentConfig_t* pAbility1 = nullptr,
         const UnitComponentConfig_t* pAbility2 = nullptr);
    ~Unit() = default;

    const char* GetId() const override;
    const std::string& GetName() const override;
    int GetBaseCost() const override;

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

    std::string m_name;
    const UnitComponentConfig_t& m_rChassis;
    const UnitComponentConfig_t& m_rWeapon;
    const UnitComponentConfig_t& m_rArmour;
    const UnitComponentConfig_t& m_rReactor;
    const UnitComponentConfig_t* m_pAbility1;
    const UnitComponentConfig_t* m_pAbility2;
};

} // namespace ac
