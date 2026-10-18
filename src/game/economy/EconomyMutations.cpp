#include "game/economy/EconomyMutations.h"

#include "game/base/BaseManager.h"
#include "game/base/HomeBaseIndex.h"
#include "game/economy/EconomyRules.h"
#include "game/economy/ResourceManager.h"
#include "game/faction/Faction.h"
#include "game/units/UnitManager.h"

#include <algorithm>
#include <vector>

namespace ac
{

void ApplyMineralSupportAtBase(BaseManager& rBase)
{
    UnitManager& rUnits = rBase.GetFaction().GetUnitManager();
    while (Unit* pToDisband = UnitToDisbandForSupport(rBase))
    {
        rUnits.DestroyUnit(*pToDisband);
    }

    ResourceManager& rResources = rBase.GetResources();
    const int charge = MineralSupportCost(rBase);
    const int bank = rResources.GetMineralBank();
    rResources.SpendMinerals(std::min(charge, bank));
}

} // namespace ac
