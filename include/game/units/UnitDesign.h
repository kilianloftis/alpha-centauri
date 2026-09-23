#pragma once

#include "game/IConstructable.h"
#include "game/units/IDesign.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

class UnitDesign : public IDesign, public IConstructable
{
public:
    UnitDesign(
        const std::vector<UnitSlotConfig_t>& rSlots,
        const std::unordered_map<std::string, const UnitComponentConfig_t*>& rComponents
    );
    ~UnitDesign() override = default;

    const std::string& GetId() const override;
    const std::string& GetName() const override;
    int GetBaseCost() const override;
    ConstructableKind_t GetConstructableKind() const override;

    const UnitComponentConfig_t* GetComponentForSlot(const std::string& rSlotId) const;

    bool HasComponent(const std::string& rComponentId) const override;
    bool IsCombatUnit() const override;

    // True when every filled component's requiredTech is discovered (empty requiredTech
    // always passes). Used when deciding whether a transferred base may keep a queued design.
    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechs) const;

    std::vector<ActiveEffect_t> CollectEffects() const override;

    // Filled (non-null) components in slot order. On-complete production
    // dispatch walks these; continuous consumers should prefer CollectEffects().
    const std::vector<const UnitComponentConfig_t*>& GetComponents() const { return m_components; }

    InteractionGridMask_t GetInteractionMask() const override { return m_interactionMask; }
    bool UsesFuel() const override;
    int MaxFuel() const override;
    UnitDomain_t GetDomain() const override;
    std::string FormatCombatRating() const override;

private:
    std::string m_id;
    std::string m_name;
    std::vector<std::pair<UnitSlotConfig_t, const UnitComponentConfig_t*>> m_slotComponents;
    std::vector<const UnitComponentConfig_t*> m_components; // non-null only; stable after construction
    bool m_bUsesFuel = false;
    int m_maxFuel = 0;
    InteractionGridMask_t m_interactionMask = 0;
};

} // namespace ac
