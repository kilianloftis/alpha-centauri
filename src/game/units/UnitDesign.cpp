#include "game/units/UnitDesign.h"
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

int UnitDesign::GetBaseCost() const
{
    int rawCost = 0;
    float costMult = 1.0f;

    for (const auto& [rSlot, pComp] : m_slotComponents)
    {
        if (!pComp) continue;
        rawCost += static_cast<int>(pComp->mineralCost * rSlot.costModifier);

        auto it = pComp->stats.find("cost_multiplier");
        if (it != pComp->stats.end())
        {
            costMult *= it->second.geometricMult;
        }
    }

    return static_cast<int>(rawCost * costMult);
}

float UnitDesign::ResolveStat_(const std::string& rStatName) const
{
    float base = 0.0f;
    float additive = 0.0f;
    float geometric = 1.0f;

    for (const UnitComponentConfig_t* pComp : m_components)
    {
        auto it = pComp->stats.find(rStatName);
        if (it != pComp->stats.end())
        {
            base     += it->second.base;
            additive += it->second.additiveMult;
            geometric *= it->second.geometricMult;
        }
    }

    return base * (1.0f + additive) * geometric;
}

bool UnitDesign::ResolveFlag_(const std::string& rFlagName) const
{
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        auto it = pComp->flags.find(rFlagName);
        if (it != pComp->flags.end() && it->second)
        {
            return true;
        }
    }
    return false;
}

std::unordered_map<std::string, float> UnitDesign::ResolveBonusTable_(const std::string& rTableName) const
{
    std::unordered_map<std::string, float> result;

    for (const UnitComponentConfig_t* pComp : m_components)
    {
        auto tableIt = pComp->bonusTables.find(rTableName);
        if (tableIt != pComp->bonusTables.end())
        {
            for (const auto& [key, val] : tableIt->second)
            {
                result[key] += val;
            }
        }
    }

    return result;
}

int UnitDesign::GetAttack() const               { return static_cast<int>(ResolveStat_("attack")); }
int UnitDesign::GetDefense() const              { return static_cast<int>(ResolveStat_("defense")); }
int UnitDesign::GetMovement() const             { return static_cast<int>(ResolveStat_("movement")); }
int UnitDesign::GetHitPoints() const            { return static_cast<int>(ResolveStat_("hit_points")); }
int UnitDesign::GetDisengageChance() const      { return static_cast<int>(ResolveStat_("disengage_chance")); }
int UnitDesign::GetFuel() const                 { return static_cast<int>(ResolveStat_("fuel")); }
int UnitDesign::GetDamageFromOutOfFuel() const  { return static_cast<int>(ResolveStat_("damage_from_out_of_fuel")); }
bool UnitDesign::IsFlight() const               { return ResolveFlag_("flight"); }
int UnitDesign::GetCargoCapacity() const        { return static_cast<int>(ResolveStat_("cargo_capacity")); }
int UnitDesign::GetDifficultTerrainCost() const { return static_cast<int>(ResolveStat_("difficult_terrain_cost")); }
bool UnitDesign::IsSingleUse() const            { return ResolveFlag_("single_use"); }

std::unordered_map<std::string, float> UnitDesign::GetTerrainAttackBonus() const
{
    return ResolveBonusTable_("terrain_attack");
}

} // namespace ac
