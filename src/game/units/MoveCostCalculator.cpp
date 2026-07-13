#include "game/units/MoveCostCalculator.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "lib/Rational.h"

#include <algorithm>
#include <string>

namespace ac
{

namespace
{

constexpr const char* k_FungusId = "Fungus";
constexpr const char* k_RoadId = "Road";

} // namespace

UnitMoveProfile_t MoveProfileFor(const Unit& rUnit)
{
    UnitMoveProfile_t profile;
    profile.ignoresDifficultTerrain = ResolveFlag(rUnit, RuleFlagId_t::IgnoreDifficultTerrain);
    profile.treatFungusAsRoad = ResolveFlag(rUnit, RuleFlagId_t::TreatFungusAsRoad);
    return profile;
}

MoveCostCalculator::MoveCostCalculator(const ImprovementRegistry& rImprovements,
                                       MovementConstants_t constants)
    : m_rImprovements(rImprovements)
    , m_constants(std::move(constants))
{
}

int MoveCostCalculator::ToFragments_(const Rational_t& rCost) const
{
    return rCost.ScaledInt(MovementConstants_t::k_moveFragmentsPerPoint);
}

int MoveCostCalculator::FeatureMoveCostFragments_(const ImprovementConfig_t& rConfig,
                                                  const UnitMoveProfile_t& rProfile,
                                                  int defaultFragments) const
{
    int cost = ToFragments_(*rConfig.moveCost);
    // Difficult terrain (Rocky, Forest, …) is anything above the default cost; Fungus
    // stays elevated unless treatFungusAsRoad handled it separately.
    if (rProfile.ignoresDifficultTerrain && rConfig.id != k_FungusId)
    {
        cost = std::min(cost, defaultFragments);
    }
    return cost;
}

int MoveCostCalculator::FungusAsRoadOverrideFragments_() const
{
    if (const ImprovementConfig_t* pRoad = m_rImprovements.Find(k_RoadId);
        pRoad && pRoad->moveCostOverride.has_value())
    {
        return ToFragments_(*pRoad->moveCostOverride);
    }
    return ToFragments_(Rational_t::Parse("1/3"));
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
                                            const UnitMoveProfile_t& rProfile,
                                            int defaultFragments,
                                            CostAggregate_t& rAgg) const
{
    if (rProfile.treatFungusAsRoad && rConfig.id == k_FungusId)
    {
        ApplyOverride_(rAgg, FungusAsRoadOverrideFragments_());
        return;
    }

    if (rConfig.moveCost.has_value())
    {
        ApplyMoveCost_(rAgg, FeatureMoveCostFragments_(rConfig, rProfile, defaultFragments));
    }

    if (rConfig.moveCostOverride.has_value())
    {
        ApplyOverride_(rAgg, ToFragments_(*rConfig.moveCostOverride));
    }
}

void MoveCostCalculator::AccumulateTileFeatures_(const Tile& rTile,
                                                 const UnitMoveProfile_t& rProfile,
                                                 int defaultFragments,
                                                 CostAggregate_t& rAgg) const
{
    for (const std::string& featureId : rTile.GetTerrainFeatureIds())
    {
        if (const ImprovementConfig_t* pConfig = m_rImprovements.Find(featureId))
        {
            AccumulateFeature_(*pConfig, rProfile, defaultFragments, rAgg);
        }
    }
    for (const ImprovementConfig_t* pConfig : rTile.GetImprovements())
    {
        if (pConfig)
        {
            AccumulateFeature_(*pConfig, rProfile, defaultFragments, rAgg);
        }
    }
}

int MoveCostCalculator::ComputeFragments(const Tile& rTile,
                                         const UnitMoveProfile_t& rProfile) const
{
    const int defaultFragments = m_constants.DefaultMoveCostFragments();
    CostAggregate_t agg;
    AccumulateTileFeatures_(rTile, rProfile, defaultFragments, agg);

    if (agg.overrideCost.has_value())
    {
        return *agg.overrideCost;
    }
    if (agg.maxCost.has_value())
    {
        return *agg.maxCost;
    }
    return defaultFragments;
}

} // namespace ac
