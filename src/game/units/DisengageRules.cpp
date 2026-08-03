#include "game/units/DisengageRules.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/BonusEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <variant>

namespace ac
{

namespace
{

bool HasHoldFamilyOrder_(const Unit& rUnit)
{
    if (!rUnit.GetOrder().has_value())
    {
        return false;
    }
    const UnitOrder_t& rOrder = *rUnit.GetOrder();
    return std::holds_alternative<HoldOrder_t>(rOrder)
           || std::holds_alternative<HoldUntilHealedOrder_t>(rOrder)
           || std::holds_alternative<HoldForTurnsOrder_t>(rOrder);
}

} // namespace

DisengageRules::DisengageRules(const MoveCostCalculator& rMoveCosts,
                               const StepEvaluator& rSteps,
                               WorldMap& rWorldMap)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
{
}

bool DisengageRules::CanDisengage(const Unit& rCandidate, const Unit& rOpponent) const
{
    if (ResolveStat(rCandidate, StatId_t::Attack) <= 0
        && !ResolveFlag(rCandidate, RuleFlagId_t::ForcesPsiCombat))
    {
        return false; // conventional and psi combat units may disengage
    }
    if (rCandidate.GetMovementPoints() <= rOpponent.GetMovementPoints())
    {
        return false; // must be strictly faster than the opponent
    }
    if (m_rWorldMap.GetUnitsOnTile(rCandidate.GetTile()).size() > 1)
    {
        return false; // stacked units hold the line together
    }
    if (rCandidate.HasAttackedThisTurn() || rCandidate.HasAttackedLastTurn())
    {
        return false;
    }
    if (rCandidate.GetDomain() == UnitDomain_t::Air || rOpponent.GetDomain() == UnitDomain_t::Air
        || rCandidate.GetDomain() == UnitDomain_t::Orbital
        || rOpponent.GetDomain() == UnitDomain_t::Orbital)
    {
        return false;
    }
    // Opponent's unit-scoped PreventsDisengage (Comm Jammer) blocks the candidate's retreat.
    if (ResolveFlag(rOpponent, RuleFlagId_t::PreventsDisengage))
    {
        return false;
    }
    if (HasHoldFamilyOrder_(rCandidate))
    {
        return false;
    }
    // Fortified position on the candidate's own tile (Base, Bunker, ...).
    if (ResolveFlag(rCandidate.GetTile(), RuleFlagId_t::PreventsDisengage))
    {
        return false;
    }
    return true;
}

std::vector<const Tile*> DisengageRules::CollectRetreatTiles(const Unit& rUnit) const
{
    std::vector<const Tile*> candidates;
    const MoveCostCalculator::Query costs = m_rMoveCosts.ForUnit(rUnit, m_rWorldMap);
    ForEachTileInChebyshevRadius(rUnit.GetTile(), m_rWorldMap, /*radius=*/1,
                                 /*includeOrigin=*/false,
        [&](const Tile* pTile, int /*distance*/)
        {
            // Legal covers terrain domain, occupants, and ZOC→ZOC violations.
            if (!m_rSteps.CanStep(rUnit, rUnit.GetTile(), *pTile))
            {
                return;
            }
            // Full-cost entries (fungus without a road / fungus-as-road) can't be retreated
            // into — the withdrawal is a single immediate move.
            if (costs.EntryTerms(*pTile).bRequiresFullCost)
            {
                return;
            }
            candidates.push_back(pTile);
        });
    return candidates;
}

} // namespace ac
