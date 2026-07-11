#pragma once

#include "game/IConstructable.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

struct EffectContext_t;

class UnitDesign : public IConstructable
{
public:
    UnitDesign(
        const std::vector<UnitSlotConfig_t>& rSlots,
        const std::unordered_map<std::string, const UnitComponentConfig_t*>& rComponents
    );
    ~UnitDesign() = default;

    const std::string& GetId() const override;
    const std::string& GetName() const override;
    int GetBaseCost() const override;

    const UnitComponentConfig_t* GetComponentForSlot(const std::string& rSlotId) const;

    // All continuous effects attached to this design's components, as ActiveEffect_t
    // instances (sourceId = component id). The single way anything outside UnitDesign
    // consumes component effects — live-unit stat resolution, faction-lane collection,
    // and tile aura scans all work on this list; the component set stays private.
    std::vector<ActiveEffect_t> CollectEffects() const;

    int GetAttack() const;
    // Attack including conditional modifiers whose condition is satisfied by ctx (e.g. a
    // terrain- or target-specific attack bonus). GetAttack() is the context-free value.
    int GetAttackAgainst(const EffectContext_t& ctx) const;
    int GetDefense() const;
    int GetMovement() const;
    int GetVision() const;
    int GetHitPoints() const;
    int GetDisengageChance() const;
    int GetFuel() const;
    int GetDamageFromOutOfFuel() const;
    bool IsFlight() const;
    int GetCargoCapacity() const;
    int GetDifficultTerrainCost() const;
    bool IsSingleUse() const;

private:
    float ResolveStat_(StatId_t statId) const;
    bool ResolveFlag_(RuleFlagId_t flagId) const;

    std::string m_id;
    std::string m_name;
    std::vector<std::pair<UnitSlotConfig_t, const UnitComponentConfig_t*>> m_slotComponents;
    std::vector<const UnitComponentConfig_t*> m_components; // non-null only; stable after construction
};

} // namespace ac
