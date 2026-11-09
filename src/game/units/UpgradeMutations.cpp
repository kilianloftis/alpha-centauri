#include "game/units/UpgradeMutations.h"

#include "game/economy/EconomyManager.h"
#include "game/faction/Faction.h"
#include "game/units/Military.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitManager.h"
#include "game/units/UpgradeRules.h"

#include <optional>

namespace ac
{

namespace
{

constexpr const char* k_HighMoraleId = "High_Morale";

int HighMoraleDelta_(const IDesign& rOld, const UnitDesign& rNew)
{
    const bool bHad = rOld.HasComponent(k_HighMoraleId);
    const bool bHas = rNew.HasComponent(k_HighMoraleId);
    return static_cast<int>(bHas) - static_cast<int>(bHad);
}

} // namespace

bool TryUpgradeUnit(Unit& rUnit, const UnitDesign& rNew)
{
    if (!CanOfferUpgrade(rUnit))
    {
        return false;
    }
    const UnitDesign* pOld = dynamic_cast<const UnitDesign*>(&rUnit.GetDesign());
    if (!pOld)
    {
        return false;
    }
    Faction& rFaction = rUnit.GetFaction();
    const std::optional<int> energy = QuoteUpgradeEnergy(rFaction, *pOld, rNew);
    if (!energy)
    {
        return false;
    }
    EconomyManager& rEconomy = rFaction.GetEconomy();
    if (!rEconomy.CanAfford(*energy))
    {
        return false;
    }

    const int moraleDelta = HighMoraleDelta_(*pOld, rNew);
    rEconomy.SpendEnergy(*energy);
    rUnit.RebindDesign(rNew);
    if (moraleDelta != 0)
    {
        rUnit.SetXp(rUnit.GetXp() + moraleDelta);
    }
    rFaction.GetMilitary().RecordBuiltComponents(rNew);
    rFaction.GetUnitManager().OnUnitDesignRebound.Emit(rUnit);
    return true;
}

} // namespace ac
