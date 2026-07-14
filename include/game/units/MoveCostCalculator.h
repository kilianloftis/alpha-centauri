#pragma once

#include "game/units/MovementConstants.h"
#include <optional>

namespace ac
{

class ImprovementRegistry;
class Tile;
class Unit;
class WorldMap;
struct ImprovementConfig_t;
struct Rational_t;

// Resolves the fragment cost to enter a tile from its terrain/improvement move_cost values
// (max cost). Any move_cost_override on the tile replaces that result entirely — even when
// the override is higher than the max cost. Among multiple overrides, the lowest wins
// (e.g. MagTube 0 beats Road 1/3). If nothing defines a cost or override, defaultMoveCost.
class MoveCostCalculator
{
public:
    explicit MoveCostCalculator(const ImprovementRegistry& rImprovements,
                                MovementConstants_t constants = {});

    // Pre-cached cost context for a specific unit. Avoids repeated flag resolution
    // when evaluating many tiles for the same unit (e.g. pathfinding).
    class Query
    {
    public:
        int ComputeFragments(const Tile& rTile) const;
        bool EndsTurnOnEntry(const Tile& rTile) const;

    private:
        friend class MoveCostCalculator;
        Query(const MoveCostCalculator& rCalc, const Unit& rUnit, const WorldMap& rWorldMap);

        struct UnitMoveProfile_t
        {
            bool ignoresDifficultTerrain = false;
            bool treatFungusAsRoad = false;
        };

        const MoveCostCalculator& m_rCalc;
        const Unit& m_rUnit;
        const WorldMap& m_rWorldMap;
        UnitMoveProfile_t m_profile;
    };

    Query ForUnit(const Unit& rUnit, const WorldMap& rWorldMap) const;

    // One-shot convenience wrappers (internally create a temporary Query).
    int ComputeFragments(const Unit& rUnit, const Tile& rTile, const WorldMap& rWorldMap) const;
    bool EndsTurnOnEntry(const Unit& rUnit, const Tile& rTile, const WorldMap& rWorldMap) const;

private:
    // Running max move_cost and lowest move_cost_override while walking a tile's features.
    // Both are unset until a feature contributes the corresponding field. An override, when
    // present, replaces maxCost in the final result (it is not min'd against maxCost).
    struct CostAggregate_t
    {
        std::optional<int> maxCost;
        std::optional<int> overrideCost;
    };

    int ComputeFragments_(const Tile& rTile,
                          const Query::UnitMoveProfile_t& rProfile) const;
    int ToFragments_(const Rational_t& rCost) const;

    int FeatureMoveCostFragments_(const ImprovementConfig_t& rConfig,
                                  const Query::UnitMoveProfile_t& rProfile,
                                  int defaultFragments) const;

    int FungusAsRoadOverrideFragments_() const;

    void ApplyOverride_(CostAggregate_t& rAgg, int overrideFragments) const;
    void ApplyMoveCost_(CostAggregate_t& rAgg, int costFragments) const;
    void AccumulateFeature_(const ImprovementConfig_t& rConfig,
                            const Query::UnitMoveProfile_t& rProfile,
                            int defaultFragments,
                            CostAggregate_t& rAgg) const;
    void AccumulateTileFeatures_(const Tile& rTile,
                                 const Query::UnitMoveProfile_t& rProfile,
                                 int defaultFragments,
                                 CostAggregate_t& rAgg) const;

    const ImprovementRegistry& m_rImprovements;
    MovementConstants_t m_constants;
};

} // namespace ac
