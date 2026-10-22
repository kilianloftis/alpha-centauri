#pragma once

#include "game/movement/MovementConstants.h"
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
    // the tightest matching move_cost MaxClamp (see MoveCostRules).
    int costFragments = 0;

    // The full cost must be banked, possibly over turns (fungus). Otherwise any positive balance
    // admits the unit.
    bool bRequiresFullCost = false;

    // Entering zeroes the unit's remaining fragments regardless of what entry cost (fungus).
    bool bEndsTurn = false;
};

// The tile-entry movement rules: the highest feature move_cost, capped by the tightest
// MaxClamp. Fungus is banked and ends the turn (docs/architecture/unit-movement-system.md).
class MoveCostRules
{
public:
    explicit MoveCostRules(const ImprovementRegistry& rImprovements,
                                MovementConstants_t constants = {});

    // Pre-cached cost context for a specific unit. Avoids repeated flag resolution
    // when evaluating many tiles for the same unit (e.g. pathfinding).
    class Query
    {
    public:
        // The entry rules for rTile, resolved from objective tile state. Execution-side
        // consumers (UnitOrderExecutor) act on these directly.
        EntryTerms_t EntryTerms(const Tile& rTile) const;

        // Planning cost in fragments. Shroud uses the default cost; end-turn entries count
        // whole turns.
        int PlannedCostFragments(const Tile& rTile) const;

    private:
        friend class MoveCostRules;
        Query(const MoveCostRules& rCalc, const Unit& rUnit, const WorldMap& rWorldMap);

        struct UnitMoveProfile_t
        {
            bool ignoresDifficultTerrain = false;
        };

        const MoveCostRules& m_rCalc;
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
