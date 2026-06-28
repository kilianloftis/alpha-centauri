#pragma once

namespace ac
{

class Unit;
class WorldMap;
struct MoveOrder_t;
struct HoldOrder_t;
struct HoldUntilHealedOrder_t;
struct HoldForTurnsOrder_t;

class UnitOrderExecutor
{
public:
    UnitOrderExecutor() = default;
    ~UnitOrderExecutor() = default;

    void Execute(Unit& rUnit, WorldMap& rWorldMap);

private:
    void Execute_(Unit& rUnit, WorldMap& rWorldMap, MoveOrder_t& rOrder);
    void Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldOrder_t& rOrder);
    void Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldUntilHealedOrder_t& rOrder);
    void Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldForTurnsOrder_t& rOrder);
};

} // namespace ac
