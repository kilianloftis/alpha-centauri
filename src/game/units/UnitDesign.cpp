#include "game/units/UnitDesign.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include <algorithm>
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
        {
            m_components.push_back(pComp);
        }
    }

    // Stable unique id from every filled slot (abilities included).
    bool bFirstId = true;
    for (const auto& [rSlot, pComp] : m_slotComponents)
    {
        if (!pComp)
        {
            continue;
        }
        if (!bFirstId)
        {
            m_id += "_";
        }
        m_id += pComp->id;
        bFirstId = false;
    }

    // Display name: Weapon Armour Chassis unit_name fragments (empty parts omitted).
    // Order is fixed SMAC-style, not slot-table order.
    auto appendUnitNamePart = [this](const UnitComponentConfig_t* pComp) {
        if (!pComp || pComp->unitName.empty())
        {
            return;
        }
        if (!m_name.empty())
        {
            m_name += " ";
        }
        m_name += pComp->unitName;
    };
    appendUnitNamePart(GetComponentForSlot("weapon"));
    appendUnitNamePart(GetComponentForSlot("armour"));
    appendUnitNamePart(GetComponentForSlot("chassis"));

    const int turnsOfFuel = ResolveStat(*this, StatId_t::TurnsOfFuel);
    m_bUsesFuel = turnsOfFuel > 0;
    m_maxFuel = m_bUsesFuel ? turnsOfFuel * GetMovementPoints() : 0;
}

const std::string& UnitDesign::GetId() const   { return m_id; }
const std::string& UnitDesign::GetName() const { return m_name; }
ConstructableKind_t UnitDesign::GetConstructableKind() const
{
    return ConstructableKind_t::Unit;
}

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

bool UnitDesign::IsCombatUnit() const
{
    return ResolveAdditiveStat(*this, StatId_t::Attack) > 0
           || ResolveFlag(*this, RuleFlagId_t::ForcesPsiCombat);
}

bool UnitDesign::HasComponent(const std::string& rComponentId) const
{
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        if (pComp && pComp->id == rComponentId)
        {
            return true;
        }
    }
    return false;
}

bool UnitDesign::IsAvailable(const std::vector<std::string>& rDiscoveredTechs) const
{
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        if (pComp && !pComp->IsAvailable(rDiscoveredTechs))
        {
            return false;
        }
    }
    return true;
}

std::vector<ActiveEffect_t> UnitDesign::CollectEffects() const
{
    return CollectUnitEffects(*this).effects;
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
        ResolveStatModifiers(FilterByStatId(allEffects, StatId_t::CostMultiplier), SeedFor(StatId_t::CostMultiplier));
    const float costMult = static_cast<float>(breakdown.total);

    return FinalizeResolvedStat(static_cast<double>(rawCost) * costMult);
}

int UnitDesign::GetStat(StatId_t statId) const
{
    return ResolveStat(*this, statId);
}

int UnitDesign::GetStat(StatId_t statId, const EffectContext_t& rCtx) const
{
    return ResolveStat(*this, statId, rCtx);
}

bool UnitDesign::GetFlag(RuleFlagId_t flagId) const
{
    return ResolveFlag(*this, flagId);
}

int UnitDesign::GetMovementPoints() const
{
    return GetStat(StatId_t::Movement);
}

int UnitDesign::GetMineralUpkeep() const
{
    return std::max(0, GetStat(StatId_t::MineralUpkeep));
}

bool UnitDesign::UsesFuel() const
{
    return m_bUsesFuel;
}

int UnitDesign::MaxFuel() const
{
    return m_maxFuel;
}

UnitDomain_t UnitDesign::GetDomain() const
{
    for (const auto& [rSlot, pComp] : m_slotComponents)
    {
        if (pComp && pComp->type == "chassis")
        {
            if (!pComp->domain.has_value())
            {
                throw std::runtime_error(
                    "Chassis component '" + pComp->id + "' has no domain");
            }
            return *pComp->domain;
        }
    }
    throw std::runtime_error("UnitDesign '" + m_id + "' has no chassis component");
}

namespace
{

std::string DecorateCombatRatingField_(
    const std::vector<std::pair<UnitSlotConfig_t, const UnitComponentConfig_t*>>& rSlotComponents,
    CombatRatingTarget_t target,
    const std::string& rValue)
{
    std::string prefixes;
    std::string suffixes;
    for (const auto& [rSlot, pComp] : rSlotComponents)
    {
        (void)rSlot;
        if (!pComp)
        {
            continue;
        }
        for (const CombatRatingModifier_t& rMod : pComp->combatRatingModifiers)
        {
            if (rMod.target != target)
            {
                continue;
            }
            prefixes += rMod.prefix;
            suffixes += rMod.suffix;
        }
    }
    return prefixes + rValue + suffixes;
}

} // namespace

std::string UnitDesign::FormatCombatRating() const
{
    const std::string attack = DecorateCombatRatingField_(
        m_slotComponents,
        CombatRatingTarget_t::Attack,
        std::to_string(ResolveAdditiveStat(*this, StatId_t::Attack)));
    const std::string defense = DecorateCombatRatingField_(
        m_slotComponents,
        CombatRatingTarget_t::Defense,
        std::to_string(ResolveAdditiveStat(*this, StatId_t::Defense)));
    const std::string movement = DecorateCombatRatingField_(
        m_slotComponents,
        CombatRatingTarget_t::Movement,
        std::to_string(ResolveAdditiveStat(*this, StatId_t::Movement)));

    std::string rating = DecorateCombatRatingField_(
        m_slotComponents,
        CombatRatingTarget_t::Rating,
        attack + "-" + defense + "-" + movement);

    for (const auto& [rSlot, pComp] : m_slotComponents)
    {
        (void)rSlot;
        if (!pComp)
        {
            continue;
        }
        for (const std::string& rLabel : pComp->combatRatingLabels)
        {
            rating += ", ";
            rating += rLabel;
        }
    }
    return rating;
}

} // namespace ac
