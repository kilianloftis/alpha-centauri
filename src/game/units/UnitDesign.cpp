#include "game/units/UnitDesign.h"
#include "lib/effects/ActiveEffect.h"
#include "lib/effects/BonusEffect.h"
#include <stdexcept>
#include <sstream>

namespace ac
{

UnitDesign::UnitDesign(
    const std::vector<UnitSlotConfig_t>& rSlots,
    const std::unordered_map<std::string, const UnitComponentConfig_t*>& rComponents
)
{
    for (const auto& rSlot : rSlots)
    {
        const UnitComponentConfig_t* pComp = nullptr;
        auto it = rComponents.find(rSlot.id);
        if (it != rComponents.end())
        {
            pComp = it->second;
        }

        if (rSlot.required && !pComp)
        {
            throw std::runtime_error("Required slot '" + rSlot.id + "' has no component assigned");
        }

        if (pComp && pComp->type != rSlot.componentType)
        {
            throw std::runtime_error(
                "Type mismatch for slot '" + rSlot.id +
                "': expected '" + rSlot.componentType + "', got '" + pComp->type + "'"
            );
        }

        m_slotComponents.push_back({rSlot, pComp});
        if (pComp)
            m_components.push_back(pComp);
    }

    bool bFirst = true;
    for (const auto& [rSlot, pComp] : m_slotComponents)
    {
        if (rSlot.required && pComp)
        {
            if (!bFirst) { m_name += " / "; m_id += "_"; }
            m_name += pComp->name;
            m_id   += pComp->id;
            bFirst = false;
        }
    }
}

const char* UnitDesign::GetId() const   { return m_id.c_str(); }
const std::string& UnitDesign::GetName() const { return m_name; }

const UnitComponentConfig_t* UnitDesign::GetComponentForSlot(const std::string& rSlotId) const
{
    for (const auto& [rSlot, pComp] : m_slotComponents)
    {
        if (rSlot.id == rSlotId)
        {
            return pComp;
        }
    }
    return nullptr;
}

std::vector<ActiveEffect_t> UnitDesign::CollectEffects() const
{
    return CollectUnitEffects(m_components);
}

int UnitDesign::GetBaseCost() const
{
    int rawCost = 0;
    for (const auto& [rSlot, pComp] : m_slotComponents)
    {
        if (!pComp) continue;
        rawCost += static_cast<int>(pComp->mineralCost * rSlot.costModifier);
    }

    const std::vector<ActiveEffect_t> allEffects = CollectEffects();
    const StatBreakdown_t breakdown =
        ResolveStatModifiers(FilterByStatId(allEffects, StatId::CostMultiplier), SeedFor(StatId::CostMultiplier));
    const float costMult = breakdown.contributions.empty() ? 1.0f : static_cast<float>(breakdown.total);

    return static_cast<int>(rawCost * costMult);
}

float UnitDesign::ResolveStat_(StatId statId) const
{
    return static_cast<float>(ResolveStatModifiers(FilterByStatId(CollectUnitEffects(m_components), statId), SeedFor(statId)).total);
}

bool UnitDesign::ResolveFlag_(RuleFlagId flagId) const
{
    for (const ActiveEffect_t& rEffect : CollectUnitEffects(m_components))
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == flagId)
        {
            return true;
        }
    }
    return false;
}

int UnitDesign::GetAttack() const               { return static_cast<int>(ResolveStat_(StatId::Attack)); }
int UnitDesign::GetDefense() const              { return static_cast<int>(ResolveStat_(StatId::Defense)); }
int UnitDesign::GetMovement() const             { return static_cast<int>(ResolveStat_(StatId::Movement)); }
int UnitDesign::GetHitPoints() const            { return static_cast<int>(ResolveStat_(StatId::HitPoints)); }
int UnitDesign::GetDisengageChance() const      { return static_cast<int>(ResolveStat_(StatId::DisengageChance)); }
int UnitDesign::GetFuel() const                 { return static_cast<int>(ResolveStat_(StatId::Fuel)); }
int UnitDesign::GetDamageFromOutOfFuel() const  { return static_cast<int>(ResolveStat_(StatId::DamageFromOutOfFuel)); }
bool UnitDesign::IsFlight() const               { return ResolveFlag_(RuleFlagId::Flight); }
int UnitDesign::GetCargoCapacity() const        { return static_cast<int>(ResolveStat_(StatId::CargoCapacity)); }
int UnitDesign::GetDifficultTerrainCost() const { return static_cast<int>(ResolveStat_(StatId::DifficultTerrainCost)); }
bool UnitDesign::IsSingleUse() const            { return ResolveFlag_(RuleFlagId::SingleUse); }

int UnitDesign::GetAttackAgainst(const EffectContext_t& ctx) const
{
    const std::vector<ActiveEffect_t> effects = CollectUnitEffects(m_components);
    return static_cast<int>(ResolveStatModifiers(FilterByStatIdInContext(effects, StatId::Attack, ctx), SeedFor(StatId::Attack)).total);
}

} // namespace ac
