#pragma once

#include <variant>

namespace ac
{

class Tile;

struct MoveOrder_t
{
    const Tile* pDestination = nullptr;
    // Fragments banked toward entering pFungusChargeTile (SMAC multi-turn fungus entry).
    const Tile* pFungusChargeTile = nullptr;
    int fungusFragmentsPaid = 0;
};

struct HoldOrder_t {};

struct HoldUntilHealedOrder_t {};

struct HoldForTurnsOrder_t
{
    int turnsRemaining;
};

using UnitOrder_t = std::variant<MoveOrder_t, HoldOrder_t, HoldUntilHealedOrder_t, HoldForTurnsOrder_t>;

} // namespace ac
