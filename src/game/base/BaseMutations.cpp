#include "game/base/BaseMutations.h"

#include "game/base/BaseManager.h"
#include "game/base/FoundBaseRules.h"
#include "game/production/ProductionManager.h"

namespace ac
{

void ApplyStartingMinerals(BaseManager& rBase, const Unit* pFoundingUnit)
{
    // Founding-only: call once from TryFoundBase after CreateBase. Transfers keep the
    // existing ProductionManager (RebindFaction); snapshot restore uses SetMineralStockpile.
    // Retool stays free while turn original is still null (see ProductionManager).
    rBase.GetProduction().SetMineralStockpile(
        ResolveStartingMinerals(rBase, pFoundingUnit));
}

} // namespace ac
