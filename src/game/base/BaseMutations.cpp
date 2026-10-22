#include "game/base/BaseMutations.h"

#include "game/base/BaseManager.h"
#include "game/base/FoundBaseRules.h"
#include "game/production/ProductionManager.h"

namespace ac
{

void ApplyStartingMinerals(BaseManager& rBase, const Unit* pFoundingUnit)
{
    // Founding only; retooling stays free while the turn original is null.
    rBase.GetProduction().SetMineralStockpile(
        ResolveStartingMinerals(rBase, pFoundingUnit));
}

} // namespace ac
