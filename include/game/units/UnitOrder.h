#pragma once

#include <string>
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

    std::string ToString() const;
};

struct HoldOrder_t
{
    std::string ToString() const;
};

struct HoldUntilHealedOrder_t
{
    std::string ToString() const;
};

struct HoldForTurnsOrder_t
{
    int turnsRemaining;

    std::string ToString() const;
};

using UnitOrder_t = std::variant<MoveOrder_t, HoldOrder_t, HoldUntilHealedOrder_t, HoldForTurnsOrder_t>;

std::string ToString(const UnitOrder_t& rOrder);

} // namespace ac
