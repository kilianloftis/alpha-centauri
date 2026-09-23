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

// The movement rules' verdict on entering one tile, resolved for a specific unit.
struct EntryTerms_t
{
    // Fragments charged on entry: the highest move_cost among the tile's features, then
    // the tightest matching move_cost MaxClamp (see MoveCostCalculator).
    int costFragments = 0;

    // The full cost must be banked — possibly across turns — before the unit may enter
    // (fungus). When false, any positive fragment balance admits the unit and the cost
    // clamps to what remains (default terrain rule).
    bool bRequiresFullCost = false;

    // Entering zeroes the unit's remaining fragments regardless of what entry cost (fungus).
    bool bEndsTurn = false;
};

// Single home of the tile-entry movement rules. For a given unit, resolves a tile into
// EntryTerms_t:
//
// Cost: the highest move_cost among the tile's terrain features and improvements. If
// nothing defines a cost, defaultMoveCost applies. IgnoreDifficultTerrain caps non-fungus
// feature costs at the default. That seed is passed to ResolveStatModifiers with the
// matching move_cost MaxClamps from CollectTileEffects (Road, River, MagTube) and the
// entering unit. The tightest clamp wins (MagTube 0 beats Road 1/3) and does not raise a
// lower cost.
//
// Fungus entry (SMAC): unless a matching move_cost MaxClamp is in play, entering fungus
// requires banking the full cost before the move and always ends the unit's turn. A
// friendly occupant on the tile waives the banking requirement (immediate entry) but not
// the forced end of turn.
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
        // The entry rules for rTile, resolved from objective tile state. Execution-side
        // consumers (UnitOrderExecutor) act on these directly.
        EntryTerms_t EntryTerms(const Tile& rTile) const;

        // Edge weight for path planning, in fragments. Shrouded (unexplored) tiles use the
        // default cost — the planner must not see rockiness / fungus / roads under shroud.
        // End-turn entries are valued in whole turns of movement: banking takes
        // ceil(cost / allotment) turns, immediate (friendly-occupant) entry exactly one.
        int PlannedCostFragments(const Tile& rTile) const;

    private:
        friend class MoveCostCalculator;
        Query(const MoveCostCalculator& rCalc, const Unit& rUnit, const WorldMap& rWorldMap);

        struct UnitMoveProfile_t
        {
            bool ignoresDifficultTerrain = false;
        };

        const MoveCostCalculator& m_rCalc;
        const Unit& m_rUnit;
        const WorldMap& m_rWorldMap;
        UnitMoveProfile_t m_profile;
    };

    Query ForUnit(const Unit& rUnit, const WorldMap& rWorldMap) const;

private:
    // Highest move_cost field on the tile. Unset when no feature defines one.
    std::optional<int> MaxFeatureCost_(const Tile& rTile,
                                       const Query::UnitMoveProfile_t& rProfile) const;

    int FeatureMoveCostFragments_(const ImprovementConfig_t& rConfig,
                                  const Query::UnitMoveProfile_t& rProfile,
                                  int defaultFragments) const;

    const ImprovementRegistry& m_rImprovements;
    MovementConstants_t m_constants;
};

} // namespace ac
