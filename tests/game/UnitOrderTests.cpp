#include "game/map/Tile.h"
#include "game/units/UnitOrder.h"
#include "game/effects/EffectEnums.h"
#include <catch2/catch_test_macros.hpp>

namespace ac
{

TEST_CASE("Unit orders have display strings", "[unit-order]")
{
    const Tile destination(4, 7);

    CHECK(MoveOrder_t{&destination}.ToString() == "Move to (4, 7)");
    CHECK(MoveOrder_t{}.ToString() == "Move");
    CHECK(HoldOrder_t{}.ToString() == "Hold");
    CHECK(HoldUntilHealedOrder_t{}.ToString() == "Hold until healed");
    CHECK(HoldForTurnsOrder_t{1}.ToString() == "Hold for 1 turn");
    CHECK(HoldForTurnsOrder_t{3}.ToString() == "Hold for 3 turns");
    CHECK(SupplyCrawlOrder_t{StatId_t::Nutrients}.ToString() == "Supply Nutrients");
    CHECK(SupplyCrawlOrder_t{StatId_t::Minerals}.ToString() == "Supply Minerals");
    CHECK(SupplyCrawlOrder_t{StatId_t::Energy}.ToString() == "Supply Energy");
}

TEST_CASE("Variant unit orders use their concrete display string", "[unit-order]")
{
    const UnitOrder_t order = HoldUntilHealedOrder_t{};

    CHECK(ToString(order) == "Hold until healed");
}

} // namespace ac
