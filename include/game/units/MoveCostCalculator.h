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
// (max cost). Any move_cost_override on the tile replaces that result entirely — even when
// the override is higher than the max cost. Among multiple overrides, the lowest wins
// (e.g. MagTube 0 beats Road 1/3). If nothing defines a cost or override, defaultMoveCost.
class MoveCostCalculator
{
public:
    explicit MoveCostCalculator(const ImprovementRegistry& rImprovements,
                                MovementConstants_t constants = {});

    // Fragment cost to enter rTile under rProfile's movement flags.
    int ComputeFragments(const Tile& rTile, const UnitMoveProfile_t& rProfile) const;

private:
    // Running max move_cost and lowest move_cost_override while walking a tile's features.
    // Both are unset until a feature contributes the corresponding field. An override, when
    // present, replaces maxCost in the final result (it is not min'd against maxCost).
    struct CostAggregate_t
    {
        std::optional<int> maxCost;
        std::optional<int> overrideCost;
    };

    int ToFragments_(const Rational_t& rCost) const;

    // Effective move_cost fragments for a feature that defines moveCost, after
    // ignoresDifficultTerrain.
    int FeatureMoveCostFragments_(const ImprovementConfig_t& rConfig,
                                  const UnitMoveProfile_t& rProfile,
                                  int defaultFragments) const;

    // Road-equivalent override used when treatFungusAsRoad applies to Fungus.
    int FungusAsRoadOverrideFragments_() const;

    void ApplyOverride_(CostAggregate_t& rAgg, int overrideFragments) const;
    void ApplyMoveCost_(CostAggregate_t& rAgg, int costFragments) const;
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
