#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/InteractionGridsConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/units/UnitDomain.h"

#include <string>
#include <vector>

namespace ac
{

// Shared blueprint for any fielded unit: player-composed UnitDesign or config NativeDesign.
// Both kinds are also IConstructable and can enter a base production queue.
class IDesign
{
public:
    virtual ~IDesign() = default;

    virtual const std::string& GetId() const = 0;
    virtual const std::string& GetName() const = 0;

    // Continuous effects as ActiveEffect_t (sourceId = component id or native id).
    virtual std::vector<ActiveEffect_t> CollectEffects() const = 0;

    // One-shot effects considered when this unit is ordered to Hold. A player design
    // gathers them from its components; a native returns the list on its config.
    virtual std::vector<TriggeredEffectConfig_t> CollectOnHoldEffects() const = 0;

    // True when additive Attack > 0 or ForcesPsiCombat (design-only resolve).
    virtual bool IsCombatUnit() const = 0;

    // True if any filled component has this id. NativeDesign always false.
    virtual bool HasComponent(const std::string& rComponentId) const = 0;

    virtual UnitDomain_t GetDomain() const = 0;

    // Intrinsic (design-only) stat / flag resolution. Prefer free ResolveStat / ResolveFlag.
    int GetStat(StatId_t statId) const;
    int GetStat(StatId_t statId, const EffectContext_t& rCtx) const;
    bool GetFlag(RuleFlagId_t flagId) const;

    int GetMovementPoints() const;
    int GetMineralUpkeep() const;

    virtual InteractionGridMask_t GetInteractionMask() const = 0;
    virtual bool UsesFuel() const = 0;
    virtual int MaxFuel() const = 0;
    virtual int GetBaseCost() const = 0;

    virtual std::string FormatCombatRating() const = 0;
};

} // namespace ac
