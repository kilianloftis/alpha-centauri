#include "game/units/UnitPlanRules.h"

#include "game/effects/EffectConfig.h"
#include "game/units/UnitDesign.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

const UnitComponentConfig_t* PartOfType_(const UnitDesign& rDesign, const std::string& rType)
{
    for (const UnitComponentConfig_t* pComponent : rDesign.GetComponents())
    {
        if (pComponent->type == rType)
        {
            return pComponent;
        }
    }
    return nullptr;
}

// SMAC's offense_value / defense_value / chassis speed: the part's own unconditional Add. A
// missing or psi part reads 0, which orders as SMAC's -1 in every comparison make_proto makes.
int PartAddAmount_(const UnitDesign& rDesign, const std::string& rType, StatId_t stat)
{
    const UnitComponentConfig_t* pPart = PartOfType_(rDesign, rType);
    if (!pPart)
    {
        return 0;
    }
    int amount = 0;
    for (const EffectConfig_t& rEffect : pPart->effects)
    {
        const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (pModifier && pModifier->stat == stat && pModifier->op == ModifierOp_t::Add
            && !pModifier->amountSource && !rEffect.condition)
        {
            const long rounded = std::lround(pModifier->amount);
            amount += static_cast<int>(rounded);
        }
    }
    return amount;
}

bool HasAbilityPlan_(const UnitDesign& rDesign, UnitPlan_t plan)
{
    return std::ranges::any_of(rDesign.GetComponents(),
                               [plan](const UnitComponentConfig_t* pComponent)
                               { return pComponent->type == "ability" && pComponent->plan == plan; });
}

std::optional<UnitPlan_t> WeaponPlan_(const UnitDesign& rDesign)
{
    const UnitComponentConfig_t* pWeapon = PartOfType_(rDesign, "weapon");
    if (!pWeapon)
    {
        return std::nullopt;
    }
    return pWeapon->plan;
}

UnitPlan_t LandPlan_(const UnitDesign& rDesign)
{
    const int offense = PartAddAmount_(rDesign, "weapon", StatId_t::Attack);
    const int defense = PartAddAmount_(rDesign, "armour", StatId_t::Defense);
    const int speed = PartAddAmount_(rDesign, "chassis", StatId_t::Movement);
    UnitPlan_t plan = defense > 1 ? UnitPlan_t::Defense : UnitPlan_t::Offense;
    if (plan == UnitPlan_t::Defense && offense >= defense)
    {
        plan = UnitPlan_t::Combat;
    }
    if (speed > 1 && offense > defense)
    {
        plan = UnitPlan_t::Offense;
    }
    if (HasAbilityPlan_(rDesign, UnitPlan_t::Offense))
    {
        plan = UnitPlan_t::Offense;
    }
    if (offense != 1 || defense != 1)
    {
        return plan;
    }
    if (speed > 1)
    {
        plan = UnitPlan_t::Recon;
    }
    if (HasAbilityPlan_(rDesign, UnitPlan_t::Defense))
    {
        plan = UnitPlan_t::Defense;
    }
    return plan;
}

} // namespace

UnitPlan_t DeriveUnitPlan(const UnitDesign& rDesign)
{
    if (const std::optional<UnitPlan_t> weaponPlan = WeaponPlan_(rDesign))
    {
        return *weaponPlan;
    }
    switch (rDesign.GetDomain())
    {
        case UnitDomain_t::Land:
            return LandPlan_(rDesign);
        case UnitDomain_t::Sea:
            return UnitPlan_t::NavalSuperiority;
        case UnitDomain_t::Air:
            return HasAbilityPlan_(rDesign, UnitPlan_t::AirSuperiority) ? UnitPlan_t::AirSuperiority
                                                                        : UnitPlan_t::Offense;
        // make_proto's missile branch: a payload names its plan above; anything else attacks.
        case UnitDomain_t::Orbital:
            return UnitPlan_t::Offense;
    }
    throw std::logic_error("DeriveUnitPlan: unhandled domain");
}

} // namespace ac
