#include "game/faction/base/resources/MineralSupport.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <vector>

namespace ac
{

namespace
{

int ResolveFreeUnitSupport_(const BaseManager& rBase)
{
    const int raw = FinalizeResolvedStat(
        ResolveStatModifiers(
            FilterBaseLevelByStatId(rBase.GetBaseEffects(), StatId_t::FreeUnitSupport),
            SeedFor(StatId_t::FreeUnitSupport))
            .total);
    return std::max(0, raw);
}

// Per home-unit mineral charge after free slots (parallel to GetUnits() order).
std::vector<int> ComputeCharges_(const BaseManager& rBase)
{
    const std::vector<Unit*>& rUnits = rBase.GetHomeUnits().GetUnits();
    std::vector<int> charges;
    charges.reserve(rUnits.size());

    int freeLeft = ResolveFreeUnitSupport_(rBase);
    for (const Unit* pUnit : rUnits)
    {
        if (!pUnit)
        {
            charges.push_back(0);
            continue;
        }
        const int cost = pUnit->GetMineralUpkeep();
        if (cost <= 0)
        {
            charges.push_back(0);
        }
        else if (freeLeft > 0)
        {
            charges.push_back(0);
            --freeLeft;
        }
        else
        {
            charges.push_back(cost);
        }
    }
    return charges;
}

int SumCharges_(const std::vector<int>& rCharges)
{
    int total = 0;
    for (int charge : rCharges)
    {
        total += charge;
    }
    return total;
}

} // namespace

void ApplyMineralSupportAtBase(BaseManager& rBase)
{
    ResourceManager& rResources = rBase.GetResources();
    UnitManager& rUnits = rBase.GetFaction().GetUnitManager();

    while (true)
    {
        const std::vector<int> charges = ComputeCharges_(rBase);
        const int total = SumCharges_(charges);
        const int bank = rResources.GetMineralBank();
        if (total <= bank)
        {
            rResources.SpendMinerals(total);
            return;
        }

        const std::vector<Unit*>& rHomeUnits = rBase.GetHomeUnits().GetUnits();
        Unit* pToDisband = nullptr;
        for (int i = static_cast<int>(rHomeUnits.size()) - 1; i >= 0; --i)
        {
            if (charges[static_cast<size_t>(i)] > 0)
            {
                pToDisband = rHomeUnits[static_cast<size_t>(i)];
                break;
            }
        }
        if (!pToDisband)
        {
            // No charged unit left to cut; spend what the bank can (should be total == 0).
            rResources.SpendMinerals(std::min(total, bank));
            return;
        }
        rUnits.DestroyUnit(*pToDisband);
    }
}

} // namespace ac
