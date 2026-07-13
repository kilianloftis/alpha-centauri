#pragma once

#include "game/units/MovementConstants.h"
#include <optional>

namespace ac
{

class ImprovementRegistry;
class Tile;
class Unit;
struct ImprovementConfig_t;
struct Rational_t;

// Effect flags that affect how terrain/improvement move costs are resolved.
struct UnitMoveProfile_t
{
    bool ignoresDifficultTerrain = false;
    // Future: Xenoempathy / native life — treat Fungus squares as roads.
    bool treatFungusAsRoad = false;
};

UnitMoveProfile_t MoveProfileFor(const Unit& rUnit);

// Resolves the fragment cost to enter a tile from its terrain/improvement move_cost values
// (max cost) and move_cost_override values (min override wins when any are present).
class MoveCostCalculator
{
public:
    explicit MoveCostCalculator(const ImprovementRegistry& rImprovements,
                                MovementConstants_t constants = {});

    // Fragment cost to enter rTile under rProfile's movement flags.
    int ComputeFragments(const Tile& rTile, const UnitMoveProfile_t& rProfile) const;

private:
    // Running max move_cost and min move_cost_override while walking a tile's features.
    struct CostAggregate_t
    {
        int maxCost = 0;
        std::optional<int> minOverride;
    };

    int ToFragments_(const Rational_t& rCost) const;

    // Base move_cost for one feature, after ignoresDifficultTerrain.
    int FeatureMoveCostFragments_(const ImprovementConfig_t& rConfig,
                                  const UnitMoveProfile_t& rProfile,
                                  int defaultFragments) const;

    // Road-equivalent override used when treatFungusAsRoad applies to Fungus.
    int FungusAsRoadOverrideFragments_() const;

    void ApplyOverride_(CostAggregate_t& rAgg, int overrideFragments) const;
    void AccumulateFeature_(const ImprovementConfig_t& rConfig,
                            const UnitMoveProfile_t& rProfile,
                            int defaultFragments,
                            CostAggregate_t& rAgg) const;
    void AccumulateTileFeatures_(const Tile& rTile,
                                 const UnitMoveProfile_t& rProfile,
                                 int defaultFragments,
                                 CostAggregate_t& rAgg) const;

    const ImprovementRegistry& m_rImprovements;
    MovementConstants_t m_constants;
};

} // namespace ac
