#pragma once

#include "game/IConstructable.h"
#include "game/units/NativeUnitConfig.h"
#include "game/units/IDesign.h"

#include <string>
#include <vector>

namespace ac
{

// Flat effect-bag design for native life. Constructable, but not slot-composed.
class NativeDesign : public IDesign, public IConstructable
{
public:
    explicit NativeDesign(const NativeUnitConfig_t& rConfig);
    ~NativeDesign() override = default;

    const std::string& GetId() const override;
    const std::string& GetName() const override;
    int GetBaseCost() const override { return m_config.mineralCost; }
    ConstructableKind_t GetConstructableKind() const override;

    std::vector<ActiveEffect_t> CollectEffects() const override;
    std::vector<TriggeredEffectConfig_t> CollectOnHoldEffects() const override;
    // Native life carries no warhead; always empty.
    std::vector<TriggeredEffectConfig_t> CollectOnDetonateEffects() const override { return {}; }
    bool IsCombatUnit() const override;
    bool HasComponent(const std::string& rComponentId) const override;
    UnitDomain_t GetDomain() const override;
    InteractionGridMask_t GetInteractionMask() const override { return m_interactionMask; }
    bool UsesFuel() const override;
    int MaxFuel() const override;
    std::string FormatCombatRating() const override;

    const NativeUnitConfig_t& GetConfig() const { return m_config; }

private:
    NativeUnitConfig_t m_config;
    bool m_bUsesFuel = false;
    int m_maxFuel = 0;
    InteractionGridMask_t m_interactionMask = 0;
};

} // namespace ac
