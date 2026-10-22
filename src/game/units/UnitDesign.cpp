#include "game/units/UnitDesign.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/InteractionResolve.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace ac
{

namespace
{

using ComponentsBySlot_t = std::unordered_map<std::string, const UnitComponentConfig_t*>;

// rSlot's component from rComponents, or nullptr for an empty optional slot.
const UnitComponentConfig_t* SlotComponent_(const UnitSlotConfig_t& rSlot,
                                            const ComponentsBySlot_t& rComponents)
{
    const auto it = rComponents.find(rSlot.id);
    const UnitComponentConfig_t* pComp = it == rComponents.end() ? nullptr : it->second;
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
    return pComp;
}

void RequireChassisFits_(const std::vector<const UnitComponentConfig_t*>& rComponents)
{
    const auto chassis = std::ranges::find(rComponents, std::string("chassis"),
                                           &UnitComponentConfig_t::type);
    const std::string chassisId = chassis == rComponents.end() ? std::string() : (*chassis)->id;
    for (const UnitComponentConfig_t* pComp : rComponents)
    {
        if (!ChassisRequirementMet(*pComp, chassisId))
        {
            throw std::runtime_error(
                "Component '" + pComp->id + "' cannot be designed on chassis '" + chassisId + "'");
        }
    }
}

// Every component, abilities included, so the id is unique to the combination.
std::string JoinComponentIds_(const std::vector<const UnitComponentConfig_t*>& rComponents)
{
    std::string id;
    for (const UnitComponentConfig_t* pComp : rComponents)
    {
        if (!id.empty())
        {
            id += "_";
        }
        id += pComp->id;
    }
    return id;
}

// SMAC orders the name weapon, armour, chassis, whatever the slot table's order.
std::string UnitNameOf_(const UnitDesign& rDesign)
{
    std::string name;
    for (const char* slotId : {"weapon", "armour", "chassis"})
    {
        const UnitComponentConfig_t* pComp = rDesign.GetComponentForSlot(slotId);
        if (!pComp || pComp->unitName.empty())
        {
            continue;
        }
        if (!name.empty())
        {
            name += " ";
        }
        name += pComp->unitName;
    }
    return name;
}

} // namespace

UnitDesign::UnitDesign(
    const std::vector<UnitSlotConfig_t>& rSlots,
    const std::unordered_map<std::string, const UnitComponentConfig_t*>& rComponents
)
{
    for (const UnitSlotConfig_t& rSlot : rSlots)
    {
        const UnitComponentConfig_t* pComp = SlotComponent_(rSlot, rComponents);
        m_slotComponents.push_back({rSlot, pComp});
        if (pComp)
        {
            m_components.push_back(pComp);
        }
    }
    RequireChassisFits_(m_components);
    m_id = JoinComponentIds_(m_components);
    m_name = UnitNameOf_(*this);

    const int turnsOfFuel = ResolveStat(*this, StatId_t::TurnsOfFuel);
    m_bUsesFuel = turnsOfFuel > 0;
    m_maxFuel = m_bUsesFuel ? turnsOfFuel * GetMovementPoints() : 0;

    for (const UnitComponentConfig_t* pComp : m_components)
    {
        m_interactionMask |= InteractionMaskOf(pComp->effects);
    }
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
    return std::ranges::contains(m_components, rComponentId, &UnitComponentConfig_t::id);
}

bool UnitDesign::IsAvailable(const std::vector<std::string>& rDiscoveredTechs) const
{
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        if (!pComp->IsAvailable(rDiscoveredTechs))
        {
            return false;
        }
    }
    return true;
}

std::vector<ActiveEffect_t> UnitDesign::CollectEffects() const
{
    std::vector<ActiveEffect_t> result;
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        AppendActiveEffects(pComp->effects, nullptr, pComp->id, result);
    }
    return result;
}

std::vector<TriggeredEffectConfig_t> UnitDesign::CollectOnHoldEffects() const
{
    std::vector<TriggeredEffectConfig_t> result;
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        result.insert(result.end(), pComp->onHoldEffects.begin(), pComp->onHoldEffects.end());
    }
    return result;
}

std::vector<TriggeredEffectConfig_t> UnitDesign::CollectOnDetonateEffects() const
{
    std::vector<TriggeredEffectConfig_t> result;
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        result.insert(result.end(), pComp->onDetonateEffects.begin(),
                      pComp->onDetonateEffects.end());
    }
    return result;
}

int UnitDesign::GetBaseCost() const
{
    int rawCost = 0;
    for (const auto& [rSlot, pComp] : m_slotComponents)
    {
        if (pComp)
        {
            rawCost += static_cast<int>(pComp->mineralCost * rSlot.costModifier);
        }
    }

    const std::vector<ActiveEffect_t> allEffects = CollectEffects();
    auto costEffects = FilterByStatId(allEffects, StatId_t::CostMultiplier);
    const double seed = SeedFor(StatId_t::CostMultiplier);
    const StatBreakdown_t breakdown = ResolveStatModifiers(costEffects, seed);
    const float costMult = static_cast<float>(breakdown.total);

    return FinalizeResolvedStat(static_cast<double>(rawCost) * costMult);
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
    for (const UnitComponentConfig_t* pComp : m_components)
    {
        if (pComp->type == "chassis")
        {
            return pComp->domain.value();
        }
    }
    throw std::runtime_error("UnitDesign '" + m_id + "' has no chassis component");
}

namespace
{

std::string DecorateCombatRatingField_(
    const std::vector<const UnitComponentConfig_t*>& rComponents,
    CombatRatingTarget_t target,
    const std::string& rValue)
{
    std::string prefixes;
    std::string suffixes;
    for (const UnitComponentConfig_t* pComp : rComponents)
    {
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
    const auto decorate = [this](CombatRatingTarget_t target, StatId_t statId)
    {
        const std::string value = std::to_string(ResolveAdditiveStat(*this, statId));
        return DecorateCombatRatingField_(m_components, target, value);
    };
    const std::string attack = decorate(CombatRatingTarget_t::Attack, StatId_t::Attack);
    const std::string defense = decorate(CombatRatingTarget_t::Defense, StatId_t::Defense);
    const std::string movement = decorate(CombatRatingTarget_t::Movement, StatId_t::Movement);

    std::string rating = DecorateCombatRatingField_(
        m_components,
        CombatRatingTarget_t::Rating,
        attack + "-" + defense + "-" + movement);

    for (const UnitComponentConfig_t* pComp : m_components)
    {
        for (const std::string& rLabel : pComp->combatRatingLabels)
        {
            rating += ", ";
            rating += rLabel;
        }
    }
    return rating;
}

} // namespace ac
