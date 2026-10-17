#pragma once

namespace ac
{

class Pathfinder;
class TileEffectsContext;
class Unit;
class WorldMap;

// Best same-faction Scramble candidate that can path to the original defender's
// tile within the effect's range and remaining move fragments, or null. Does not mutate.
// Callers assign a MoveOrder and Execute to walk the path (UnitOrderExecutor::TryAttack).
Unit* FindScrambler(const Unit& rAttacker,
                              Unit& rOriginalDefender,
                              const WorldMap& rWorldMap,
                              const TileEffectsContext& rTileEffects,
                              const Pathfinder& rPathfinder);

} // namespace ac
