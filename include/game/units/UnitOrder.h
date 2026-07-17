#pragma once

#include <variant>

namespace ac
{

class Tile;

struct MoveOrder_t
{
    const Tile* pDestination = nullptr;
    // Fragments banked toward a bRequiresFullCost entry (SMAC multi-turn fungus entry).
    // Reset whenever the unit enters a tile or charges a different one.
    const Tile* pChargeTile = nullptr;
    int chargeFragmentsPaid = 0;
};

struct HoldOrder_t {};

struct HoldUntilHealedOrder_t {};

struct HoldForTurnsOrder_t
{
    int turnsRemaining;
};

using UnitOrder_t = std::variant<MoveOrder_t, HoldOrder_t, HoldUntilHealedOrder_t, HoldForTurnsOrder_t>;

} // namespace ac
