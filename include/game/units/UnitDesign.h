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

    // Intrinsic (component-only) stat / flag resolution. Prefer the free ResolveStat /
    // ResolveFlag overloads; these forward to them.
    int GetStat(StatId_t statId) const;
    int GetStat(StatId_t statId, const EffectContext_t& rCtx) const;
    bool GetFlag(RuleFlagId_t flagId) const;

    // Chassis Movement stat in move-points (not fragments).
    int GetMovementPoints() const;

    // Domain of the design's chassis component (required on every valid design).
    UnitDomain_t GetDomain() const;

    // SMAC-style combat rating: additive A-D-M with component display annotations
    // (e.g. "2~-<3r>-1*2, ECM").
    std::string FormatCombatRating() const;

private:
    std::string m_id;
    std::string m_name;
    std::vector<std::pair<UnitSlotConfig_t, const UnitComponentConfig_t*>> m_slotComponents;
    std::vector<const UnitComponentConfig_t*> m_components; // non-null only; stable after construction
};

} // namespace ac
