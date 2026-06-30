#pragma once

#include "game/IConstructable.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

class UnitDesign : public IConstructable
{
public:
    UnitDesign(
        const std::vector<UnitSlotConfig_t>& rSlots,
        const std::unordered_map<std::string, const UnitComponentConfig_t*>& rComponents
    );
    ~UnitDesign() = default;

    const char* GetId() const override;
    const std::string& GetName() const override;
    int GetBaseCost() const override;

    const UnitComponentConfig_t* GetComponentForSlot(const std::string& rSlotId) const;

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
    std::vector<std::pair<UnitSlotConfig_t, const UnitComponentConfig_t*>> m_slotComponents;
    std::vector<const UnitComponentConfig_t*> m_components; // non-null only, for stat resolution
};

} // namespace ac
