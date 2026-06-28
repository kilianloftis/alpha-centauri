#pragma once

#include <variant>

namespace ac
{

class Tile;

struct MoveOrder_t
{
    Tile* pDestination;
};

struct HoldOrder_t {};

struct HoldUntilHealedOrder_t {};

struct HoldForTurnsOrder_t
{
    int turnsRemaining;
};

using UnitOrder_t = std::variant<MoveOrder_t, HoldOrder_t, HoldUntilHealedOrder_t, HoldForTurnsOrder_t>;

} // namespace ac
