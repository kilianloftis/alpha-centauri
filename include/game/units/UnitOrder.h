#pragma once

#include "game/effects/EffectEnums.h"
#include <string>
#include <variant>

namespace ac
{

class Tile;

// Result of advancing a unit's current order one execution step (PlayerActions pass or
// an immediate use-action). Callers use this instead of inspecting post-hoc order/unit state.
enum class OrderProgress_t
{
    Continue, // order persists into next turn
    Complete, // order finished; unit survives
    Expended, // order finished by consuming the unit (SingleUse)
};

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

// Park on the current tile and harvest one resource type for the unit's home base.
// resource must be Nutrients, Minerals, or Energy.
struct SupplyCrawlOrder_t
{
    StatId_t resource = StatId_t::Nutrients;

    std::string ToString() const;
};

// Multi-turn Former project. improvementId names an ImprovementConfig_t entry
// (placeable or terraform action). turnsRemaining counts down each PlayerActions pass.
struct TerraformOrder_t
{
    std::string improvementId;
    int turnsRemaining = 0;

    std::string ToString() const;
};

using UnitOrder_t = std::variant<
    MoveOrder_t,
    HoldOrder_t,
    HoldUntilHealedOrder_t,
    HoldForTurnsOrder_t,
    SupplyCrawlOrder_t,
    TerraformOrder_t>;

std::string ToString(const UnitOrder_t& rOrder);

} // namespace ac
