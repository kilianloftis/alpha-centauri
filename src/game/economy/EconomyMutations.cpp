#include "game/economy/EconomyMutations.h"

#include "game/base/BaseManager.h"
#include "game/base/HomeBaseIndex.h"
#include "game/economy/EconomyRules.h"
#include "game/economy/ResourceManager.h"
#include "game/faction/Faction.h"
#include "game/units/UnitManager.h"

#include <algorithm>
#include <numeric>
#include <vector>

namespace ac
{

void ApplyMineralSupportAtBase(BaseManager& rBase)
{
    ResourceManager& rResources = rBase.GetResources();
    UnitManager& rUnits = rBase.GetFaction().GetUnitManager();

    while (true)
    {
        const std::vector<int> charges = MineralSupportCharges(rBase);
        const int total = std::accumulate(charges.begin(), charges.end(), 0);
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
