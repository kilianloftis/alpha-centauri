#include "game/combat/CollateralMutations.h"

#include "game/combat/CaptureMutations.h"
#include "game/combat/CollateralRules.h"
#include "game/faction/Faction.h"
#include "game/units/Unit.h"

#include <optional>
#include <vector>

namespace ac
{

namespace
{

void RemoveCasualty_(UnitPositionIndex& rPositions, Unit& rAttacker, Unit& rCasualty,
                     std::vector<UnitId_t>& rCaptured)
{
    const std::optional<UnitId_t> capturedId =
        RemoveDefeatedUnit(rPositions, rAttacker, rCasualty);
    if (capturedId)
    {
        rCaptured.push_back(*capturedId);
    }
}

void ApplyHit_(UnitPositionIndex& rPositions, Unit& rAttacker, const CollateralHit_t& rHit,
               std::vector<UnitId_t>& rCaptured)
{
    Unit& rOccupant = *rHit.pOccupant;
    if (rHit.bDestroyed)
    {
        RemoveCasualty_(rPositions, rAttacker, rOccupant, rCaptured);
        return;
    }
    if (rHit.damage == 0)
    {
        return;
    }
    rOccupant.SetCurrentHp(rOccupant.GetCurrentHp() - rHit.damage);
    if (rOccupant.GetCurrentHp() <= 0)
    {
        RemoveCasualty_(rPositions, rAttacker, rOccupant, rCaptured);
    }
}

} // namespace

std::vector<UnitId_t> ApplyStackCollateral(UnitPositionIndex& rPositions, Unit& rAttacker,
                                           Unit& rDefender)
{
    std::vector<UnitId_t> captured;
    const std::vector<CollateralHit_t> plan = PlanStackCollateral(rAttacker, rDefender);
    for (const CollateralHit_t& rHit : plan)
    {
        ApplyHit_(rPositions, rAttacker, rHit, captured);
    }

    const std::vector<Unit*> unprotected =
        NonCombatantsLeftUnprotected(rAttacker, rDefender, plan);
    for (Unit* pNonCombatant : unprotected)
    {
        RemoveCasualty_(rPositions, rAttacker, *pNonCombatant, captured);
    }
    return captured;
}

} // namespace ac
