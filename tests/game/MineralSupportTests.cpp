// Mineral support: home units charge mineral_upkeep against the turn mineral bank;
// free_unit_support covers oldest positive-cost units; shortfall disbands newest charged first.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

bool UnitStillLive_(const Faction& rFaction, UnitId_t unitId)
{
    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        if (rUnit.GetUnitId() == unitId)
        {
            return true;
        }
    }
    return false;
}

void LeaveMineralBank_(BaseManager& rBase, int desired)
{
    if (!rBase.GetBuildingManager().HasBuilding("mineral_cache"))
    {
        rBase.GetBuildingManager().AddBuilding("mineral_cache");
    }
    rBase.ProduceResources();
    ResourceManager& rResources = rBase.GetResources();
    const int bank = rResources.GetMineralBank();
    REQUIRE(bank >= desired);
    rResources.SpendMinerals(bank - desired);
    REQUIRE(rResources.GetMineralBank() == desired);
}

} // namespace

TEST_CASE("UnitDesign mineral upkeep floors at zero", "[unit][support]")
{
    EffectPool pool;
    UnitComponentConfig_t chassis;
    chassis.id = "upkeep_chassis";
    chassis.type = "chassis";
    chassis.domain = UnitDomain_t::Land;
    chassis.effects = {
        pool.StatMod(StatId_t::Movement, 1.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
        pool.StatMod(StatId_t::HitPoints, 10.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
        pool.StatMod(StatId_t::MineralUpkeep, 1.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
        pool.StatMod(StatId_t::MineralUpkeep, -5.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
    };

    const std::vector<UnitSlotConfig_t> slots = {
        {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
    };
    const std::unordered_map<std::string, const UnitComponentConfig_t*> assigned = {
        {"chassis", &chassis},
    };
    const UnitDesign design(slots, assigned);

    CHECK(design.GetStat(StatId_t::MineralUpkeep) == -4);
    CHECK(design.GetMineralUpkeep() == 0);
}

TEST_CASE("Fixture chassis reports mineral upkeep of 1", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    CHECK(unit.GetDesign().GetMineralUpkeep() == 1);
    CHECK(unit.GetMineralUpkeep() == 1);
}

TEST_CASE("Clean reactor ability zeroes mineral upkeep without taking a free slot",
          "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& freeCost = fixture.MakeUnit(faction, 5, 4, {"test_chassis", "test_clean_reactor"}, &base);
    Unit& paid = fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    CHECK(freeCost.GetMineralUpkeep() == 0);
    CHECK(paid.GetMineralUpkeep() == 1);

    base.GetBuildingManager().AddBuilding("free_support_depot"); // +2 free slots
    LeaveMineralBank_(base, 0);

    base.ApplyMineralSupport();

    // Zero-upkeep unit does not consume a free slot; one free slot covers `paid`.
    CHECK(base.GetHomeUnits().GetUnits().size() == 2);
    CHECK(base.GetResources().GetMineralBank() == 0);
}

TEST_CASE("Free unit support covers oldest positive-upkeep home units", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& oldest = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    Unit& middle = fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    Unit& newest = fixture.MakeUnit(faction, 7, 4, {"test_chassis"}, &base);
    const UnitId_t newestId = newest.GetUnitId();

    base.GetBuildingManager().AddBuilding("free_support_depot"); // +2 free
    LeaveMineralBank_(base, 0);

    base.ApplyMineralSupport();

    // Two free slots cover oldest+middle; newest costs 1 with bank 0 → disbanded.
    REQUIRE(base.GetHomeUnits().GetUnits().size() == 2);
    CHECK(base.GetHomeUnits().GetUnits()[0] == &oldest);
    CHECK(base.GetHomeUnits().GetUnits()[1] == &middle);
    CHECK(base.GetResources().GetMineralBank() == 0);
    CHECK_FALSE(UnitStillLive_(faction, newestId));
}

TEST_CASE("FactionUnits mineral_upkeep Add increases support charge", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& unit = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    CHECK(unit.GetMineralUpkeep() == 1);

    base.GetBuildingManager().AddBuilding("support_cost_penalty");
    CHECK(unit.GetMineralUpkeep() == 2);

    LeaveMineralBank_(base, 2);
    base.ApplyMineralSupport();

    CHECK(base.GetHomeUnits().GetUnits().size() == 1);
    CHECK(base.GetResources().GetMineralBank() == 0);
}

TEST_CASE("Insufficient minerals disband newest charged home units first", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& a = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    Unit& b = fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    Unit& c = fixture.MakeUnit(faction, 7, 4, {"test_chassis"}, &base);
    const UnitId_t bId = b.GetUnitId();
    const UnitId_t cId = c.GetUnitId();

    LeaveMineralBank_(base, 1);
    base.ApplyMineralSupport();

    REQUIRE(base.GetHomeUnits().GetUnits().size() == 1);
    CHECK(base.GetHomeUnits().GetUnits().front() == &a);
    CHECK(base.GetResources().GetMineralBank() == 0);
    CHECK_FALSE(UnitStillLive_(faction, bId));
    CHECK_FALSE(UnitStillLive_(faction, cId));
}

TEST_CASE("Mineral support leaves remainder for production", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);

    LeaveMineralBank_(base, 5);
    base.ApplyMineralSupport();

    CHECK(base.GetHomeUnits().GetUnits().size() == 2);
    CHECK(base.GetResources().GetMineralBank() == 3);
}

TEST_CASE("SpendMinerals rejects overspend", "[unit][support][resources]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    LeaveMineralBank_(base, 1);
    CHECK_THROWS(base.GetResources().SpendMinerals(2));
    CHECK_THROWS(base.GetResources().SpendMinerals(-1));
    CHECK(base.GetResources().GetMineralBank() == 1);
}
