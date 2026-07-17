#include "game/units/MoveCostCalculator.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/FactionExploredMap.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"

#include <algorithm>

namespace ac
{

namespace
{

constexpr const char* k_FungusId = "Fungus";
constexpr const char* k_RoadId = "Road";

} // namespace

// --- Query -------------------------------------------------------------------

MoveCostCalculator::Query::Query(const MoveCostCalculator& rCalc,
                                 const Unit& rUnit,
                                 const WorldMap& rWorldMap)
    : m_rCalc(rCalc)
    , m_rUnit(rUnit)
    , m_rWorldMap(rWorldMap)
{
    m_profile.ignoresDifficultTerrain = ResolveFlag(rUnit, RuleFlagId_t::IgnoreDifficultTerrain);
    m_profile.treatFungusAsRoad = ResolveFlag(rUnit, RuleFlagId_t::TreatFungusAsRoad);
}

EntryTerms_t MoveCostCalculator::Query::EntryTerms(const Tile& rTile) const
{
    const CostAggregate_t agg = m_rCalc.AggregateTileFeatures_(rTile, m_profile);

    EntryTerms_t terms;
    terms.costFragments = agg.overrideCost.has_value()
        ? *agg.overrideCost
        : agg.maxCost.value_or(m_rCalc.m_constants.DefaultMoveCostFragments());

    // An override in play (Road / MagTube built on the tile, or TreatFungusAsRoad mapping
    // fungus to Road's override) negates the fungus entry rules along with the cost.
    if (rTile.GetHasFungus() && !agg.overrideCost.has_value())
    {
        terms.bEndsTurn = true;
        terms.bRequiresFullCost = !HasFriendlyOccupant(m_rUnit, rTile, m_rWorldMap);
    }
    return terms;
}

int MoveCostCalculator::Query::PlannedCostFragments(const Tile& rTile) const
{
    const FactionExploredMap& rExplored = m_rUnit.GetFaction().GetExploredMap();
    if (rExplored.IsSized() && !rExplored.IsExplored(rTile))
    {
        return m_rCalc.m_constants.DefaultMoveCostFragments();
    }

    const EntryTerms_t terms = EntryTerms(rTile);
    if (!terms.bEndsTurn)
    {
        return terms.costFragments;
    }

    const int allotment =
        m_rUnit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
    if (allotment <= 0)
    {
        return terms.costFragments;
    }
    const int bankingTurns = (terms.costFragments + allotment - 1) / allotment;
    const int turns = terms.bRequiresFullCost ? std::max(bankingTurns, 1) : 1;
    return turns * allotment;
}

// --- MoveCostCalculator ------------------------------------------------------

MoveCostCalculator::MoveCostCalculator(const ImprovementRegistry& rImprovements,
                                       MovementConstants_t constants)
    : m_rImprovements(rImprovements)
    , m_constants(std::move(constants))
{
}

MoveCostCalculator::Query MoveCostCalculator::ForUnit(const Unit& rUnit,
                                                      const WorldMap& rWorldMap) const
{
    return Query(*this, rUnit, rWorldMap);
}

int MoveCostCalculator::FeatureMoveCostFragments_(const ImprovementConfig_t& rConfig,
                                                  const Query::UnitMoveProfile_t& rProfile,
                                                  int defaultFragments) const
{
    int cost = *rConfig.moveCostFragments;
    // Difficult terrain (Rocky, Forest, …) is anything above the default cost; Fungus
    // stays elevated unless treatFungusAsRoad handled it separately.
    if (rProfile.ignoresDifficultTerrain && rConfig.id != k_FungusId)
    {
        cost = std::min(cost, defaultFragments);
    }
    return cost;
}

std::optional<int> MoveCostCalculator::FungusAsRoadOverrideFragments_() const
{
    const ImprovementConfig_t* pRoad = m_rImprovements.Find(k_RoadId);
    if (!pRoad || !pRoad->moveCostOverrideFragments.has_value())
        return std::nullopt;
    return *pRoad->moveCostOverrideFragments;
}

void MoveCostCalculator::ApplyOverride_(CostAggregate_t& rAgg, int overrideFragments) const
{
    // Among overrides only — never compared against maxCost here.
    rAgg.overrideCost = rAgg.overrideCost.has_value()
        ? std::min(*rAgg.overrideCost, overrideFragments)
        : overrideFragments;
}

void MoveCostCalculator::ApplyMoveCost_(CostAggregate_t& rAgg, int costFragments) const
{
    rAgg.maxCost = rAgg.maxCost.has_value()
        ? std::max(*rAgg.maxCost, costFragments)
        : costFragments;
}

void MoveCostCalculator::AccumulateFeature_(const ImprovementConfig_t& rConfig,
                                            const Query::UnitMoveProfile_t& rProfile,
                                            int defaultFragments,
                                            CostAggregate_t& rAgg) const
{
    if (rProfile.treatFungusAsRoad && rConfig.id == k_FungusId)
    {
        if (const std::optional<int> roadOverride = FungusAsRoadOverrideFragments_())
        {
            ApplyOverride_(rAgg, *roadOverride);
            return;
        }
        // No Road override configured — fall through to Fungus's own costs.
    }

    if (rConfig.moveCostFragments.has_value())
    {
        ApplyMoveCost_(rAgg, FeatureMoveCostFragments_(rConfig, rProfile, defaultFragments));
    }

    if (rConfig.moveCostOverrideFragments.has_value())
    {
        ApplyOverride_(rAgg, *rConfig.moveCostOverrideFragments);
    }
}

MoveCostCalculator::CostAggregate_t MoveCostCalculator::AggregateTileFeatures_(
    const Tile& rTile, const Query::UnitMoveProfile_t& rProfile) const
{
    const int defaultFragments = m_constants.DefaultMoveCostFragments();
    CostAggregate_t agg;
    for (const ImprovementConfig_t* pConfig : rTile.GetTerrainFeatures())
    {
        if (pConfig)
        {
            AccumulateFeature_(*pConfig, rProfile, defaultFragments, agg);
        }
    }
    for (const ImprovementConfig_t* pConfig : rTile.GetImprovements())
    {
        if (pConfig)
        {
            AccumulateFeature_(*pConfig, rProfile, defaultFragments, agg);
        }
    }
    return agg;
}

} // namespace ac
