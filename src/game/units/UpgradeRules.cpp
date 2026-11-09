#include "game/units/UpgradeRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/movement/MovementConstants.h"
#include "game/natives/NativeDesign.h"
#include "game/production/ProductionRules.h"
#include "game/units/Military.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UpgradeCostCalculator.h"

#include <memory>
#include <stdexcept>
#include <variant>

namespace ac
{

namespace
{

// SMAC Weapon/Armor table Add bytes: sum of Add modifiers on that component for the stat.
int ComponentAdditiveStat_(const UnitComponentConfig_t* pComponent, StatId_t statId)
{
    if (!pComponent)
    {
        return 0;
    }
    double total = SeedFor(statId);
    for (const EffectConfig_t& rEffect : pComponent->effects)
    {
        const StatModifierEffect_t* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (pMod && pMod->stat == statId && pMod->op == ModifierOp_t::Add)
        {
            total += pMod->amount;
        }
    }
    return FinalizeResolvedStat(total);
}

const UnitComponentConfig_t* ComponentOfType_(const UnitDesign& rDesign, const char* pType)
{
    for (const UnitComponentConfig_t* pComponent : rDesign.GetComponents())
    {
        if (pComponent && pComponent->type == pType)
        {
            return pComponent;
        }
    }
    return nullptr;
}

int WeaponOffense_(const UnitDesign& rDesign)
{
    return ComponentAdditiveStat_(ComponentOfType_(rDesign, "weapon"), StatId_t::Attack);
}

int ArmorDefense_(const UnitDesign& rDesign)
{
    return ComponentAdditiveStat_(ComponentOfType_(rDesign, "armour"), StatId_t::Defense);
}

int TypeMineralCost_(const UnitDesign& rDesign, const char* pType)
{
    const UnitComponentConfig_t* pComponent = ComponentOfType_(rDesign, pType);
    return pComponent ? pComponent->mineralCost : 0;
}

bool WeaponCompatible_(const UnitDesign& rOld, const UnitDesign& rNew)
{
    const int oldOffense = WeaponOffense_(rOld);
    if (oldOffense == 0)
    {
        const UnitComponentConfig_t* pOldWeapon = ComponentOfType_(rOld, "weapon");
        const UnitComponentConfig_t* pNewWeapon = ComponentOfType_(rNew, "weapon");
        if (!pOldWeapon || !pNewWeapon)
        {
            return false;
        }
        return pOldWeapon->id == pNewWeapon->id;
    }
    return WeaponOffense_(rNew) >= oldOffense;
}

bool ArmorCompatible_(const UnitDesign& rOld, const UnitDesign& rNew)
{
    const int oldDefense = ArmorDefense_(rOld);
    const int newDefense = ArmorDefense_(rNew);
    if (newDefense >= oldDefense)
    {
        return true;
    }
    // Human Console_upgrade exception: trade armor for strictly better guns.
    if (newDefense == 1)
    {
        return false;
    }
    const int oldOffense = WeaponOffense_(rOld);
    if (oldOffense == 0)
    {
        return false;
    }
    return WeaponOffense_(rNew) > oldOffense;
}

UpgradeCostInputs_t BuildCostInputs_(const Faction& rFaction, const UnitDesign& rOld,
                                     const UnitDesign& rNew)
{
    UpgradeCostInputs_t inputs;
    inputs.oldWeaponMineralCost = TypeMineralCost_(rOld, "weapon");
    inputs.newWeaponMineralCost = TypeMineralCost_(rNew, "weapon");
    inputs.oldArmorMineralCost = TypeMineralCost_(rOld, "armour");
    inputs.newArmorMineralCost = TypeMineralCost_(rNew, "armour");
    inputs.newBaseCost = rNew.GetBaseCost();
    inputs.bTargetUnprototyped = IsPrototype(rFaction, rNew);
    return inputs;
}

} // namespace

std::set<std::string> UpgradeClasses(const UnitDesign& rDesign)
{
    std::set<std::string> upgradeClasses;
    for (const UnitComponentConfig_t* pComponent : rDesign.GetComponents())
    {
        if (pComponent && !pComponent->upgradeClass.empty())
        {
            upgradeClasses.insert(pComponent->upgradeClass);
        }
    }
    return upgradeClasses;
}

bool CanOfferUpgrade(const Unit& rUnit)
{
    if (rUnit.GetDesign().IsNativeLife())
    {
        return false;
    }
    if (dynamic_cast<const NativeDesign*>(&rUnit.GetDesign()) != nullptr)
    {
        return false;
    }
    const int maxFragments =
        rUnit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
    return rUnit.GetMoveFragmentsRemaining() == maxFragments;
}

bool IsValidUpgrade(const Faction& rFaction, const UnitDesign& rOld, const UnitDesign& rNew)
{
    if (&rOld == &rNew || rOld.GetId() == rNew.GetId())
    {
        return false;
    }
    if (rFaction.GetMilitary().GetDesign(rNew.GetId()) != &rNew)
    {
        return false;
    }
    if (UpgradeClasses(rOld) != UpgradeClasses(rNew))
    {
        return false;
    }
    if (!WeaponCompatible_(rOld, rNew))
    {
        return false;
    }
    if (!ArmorCompatible_(rOld, rNew))
    {
        return false;
    }
    return true;
}

std::vector<const UnitDesign*> UpgradeTargets(const Faction& rFaction, const UnitDesign& rOld)
{
    std::vector<const UnitDesign*> targets;
    for (const std::unique_ptr<IDesign>& pDesign : rFaction.GetMilitary().GetDesigns())
    {
        const UnitDesign* pUnitDesign = dynamic_cast<const UnitDesign*>(pDesign.get());
        if (!pUnitDesign)
        {
            continue;
        }
        if (IsValidUpgrade(rFaction, rOld, *pUnitDesign))
        {
            targets.push_back(pUnitDesign);
        }
    }
    return targets;
}

std::optional<int> QuoteUpgradeEnergy(const Faction& rFaction, const UnitDesign& rOld,
                                      const UnitDesign& rNew)
{
    if (!IsValidUpgrade(rFaction, rOld, rNew))
    {
        return std::nullopt;
    }
    const UpgradeCostCalculator* pCalc = CurrentGameState().GetGameData().upgradeCostCalculator.get();
    if (!pCalc)
    {
        throw std::runtime_error("QuoteUpgradeEnergy: no UpgradeCostCalculator on GameDataContext");
    }
    const int energy = pCalc->UpgradeEnergyCost(BuildCostInputs_(rFaction, rOld, rNew));
    const double scale = ResolveFactionStat(rFaction.GetActiveEffects(), StatId_t::UpgradeCostScale,
                                            SeedFor(StatId_t::UpgradeCostScale), nullptr);
    // Truncate toward zero like SMAC's integer Nano Factory half.
    return static_cast<int>(static_cast<double>(energy) * scale);
}

} // namespace ac
