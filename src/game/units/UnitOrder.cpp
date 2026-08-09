#include "game/units/UnitOrder.h"
#include "game/map/Tile.h"
#include <string>
#include <variant>

namespace ac
{

std::string MoveOrder_t::ToString() const
{
    if (!pDestination)
    {
        return "Move";
    }

    return "Move to ("
        + std::to_string(pDestination->GetX())
        + ", "
        + std::to_string(pDestination->GetY())
        + ")";
}

std::string HoldOrder_t::ToString() const
{
    return "Hold";
}

std::string HoldUntilHealedOrder_t::ToString() const
{
    return "Hold until healed";
}

std::string HoldForTurnsOrder_t::ToString() const
{
    return "Hold for " + std::to_string(turnsRemaining)
        + (turnsRemaining == 1 ? " turn" : " turns");
}

std::string SkipTurnOrder_t::ToString() const
{
    return "Skip turn";
}

std::string SupplyCrawlOrder_t::ToString() const
{
    switch (resource)
    {
        case StatId_t::Nutrients: return "Supply Nutrients";
        case StatId_t::Minerals:  return "Supply Minerals";
        case StatId_t::Energy:    return "Supply Energy";
        default:                  return "Supply";
    }
}

std::string TerraformOrder_t::ToString() const
{
    return "Terraform " + improvementId
        + " (" + std::to_string(turnsRemaining)
        + (turnsRemaining == 1 ? " turn)" : " turns)");
}

std::string ToString(const UnitOrder_t& rOrder)
{
    return std::visit([](const auto& rConcreteOrder) {
        return rConcreteOrder.ToString();
    }, rOrder);
}

} // namespace ac
