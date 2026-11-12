#include "game/combat/DisengageRules.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MoveCostRules.h"
#include "game/movement/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <cmath>
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

DisengageRules::DisengageRules(const MoveCostRules& rMoveCosts,
                               const StepEvaluator& rSteps,
                               WorldMap& rWorldMap)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
{
}

bool DisengageRules::CanDisengage(const Unit& rCandidate, const Unit& rOpponent) const
{
    if (!rCandidate.IsCombatUnit())
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
    if (rCandidate.Latches().HasAttackedThisTurn() || rCandidate.Latches().HasAttackedLastTurn())
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

std::optional<int> DisengageRules::DisengageThreshold(const Unit& rCandidate,
                                                      const Unit& rOpponent) const
{
    if (!CanDisengage(rCandidate, rOpponent))
    {
        return std::nullopt;
    }
    const EffectContext_t ctx{&rCandidate.GetTile(), CombatRole_t::Defender};
    const UnitEffects_t effects = CollectLiveUnitEffects(rCandidate);
    const StatId_t stat = StatId_t::DisengageHpLimit;
    const double limit = ResolveUnitStat(effects, stat, SeedFor(stat), &ctx);
    const int startHp = rCandidate.GetCurrentHp();
    // ceil so 0.5 matches SMAC (HP+1)/2; floor(HP×0.5) would undershoot odd HP.
    const int threshold =
        static_cast<int>(std::ceil(static_cast<double>(startHp) * limit - 1e-12));
    if (threshold <= 0)
    {
        return std::nullopt;
    }
    return threshold;
}

std::vector<const Tile*> DisengageRules::CollectRetreatTiles(const Unit& rUnit) const
{
    std::vector<const Tile*> candidates;
    const MoveCostRules::Query costs = m_rMoveCosts.ForUnit(rUnit, m_rWorldMap);
    ForEachNeighborInChebyshevRadius(rUnit.GetTile(), m_rWorldMap, /*radius=*/1,
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
