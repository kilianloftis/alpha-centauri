#include "game/economy/EconomyRules.h"

#include "game/base/BaseManager.h"
#include "game/base/HomeBaseIndex.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <numeric>
#include <vector>

namespace ac
{

namespace
{

int ResolveFreeUnitSupport_(const BaseManager& rBase)
{
    const int raw = FinalizeResolvedStat(ResolveBaseStat(
        rBase.GetBaseEffects(), StatId_t::FreeUnitSupport, SeedFor(StatId_t::FreeUnitSupport)));
    return std::max(0, raw);
}

} // namespace

std::vector<int> MineralSupportCharges(const BaseManager& rBase)
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
        int cost = pUnit->GetMineralUpkeep();
        if (pUnit->GetDesign().IsNativeLife() && pUnit->GetTile().HasFeature(ImprovementIds::k_Fungus))
        {
            cost = 0;
        }
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

int MineralSupportCost(const BaseManager& rBase)
{
    const std::vector<int> charges = MineralSupportCharges(rBase);
    return std::accumulate(charges.begin(), charges.end(), 0);
}

} // namespace ac
