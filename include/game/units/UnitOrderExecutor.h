#pragma once

namespace ac
{

class Unit;
class WorldMap;
class ImprovementRegistry;
struct MoveOrder_t;
struct HoldOrder_t;
struct HoldUntilHealedOrder_t;
struct HoldForTurnsOrder_t;

class UnitOrderExecutor
{
public:
    UnitOrderExecutor() = default;
    ~UnitOrderExecutor() = default;

    void Execute(Unit& rUnit, WorldMap& rWorldMap, const ImprovementRegistry& rImprovements);

private:
    void Execute_(Unit& rUnit, WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                  MoveOrder_t& rOrder);
    void Execute_(Unit& rUnit, WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                  HoldOrder_t& rOrder);
    void Execute_(Unit& rUnit, WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                  HoldUntilHealedOrder_t& rOrder);
    void Execute_(Unit& rUnit, WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                  HoldForTurnsOrder_t& rOrder);
};

} // namespace ac
