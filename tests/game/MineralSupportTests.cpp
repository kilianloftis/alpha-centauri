// Mineral support: home units charge mineral_upkeep against the turn mineral bank;
// free_unit_support covers oldest positive-cost units; shortfall disbands newest charged first.
// Support SE level 0 emits free_unit_support +2; other levels emit absolute slot counts.

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

    // Support 0 emits 2 free slots — enough for `paid` without a facility.
    LeaveMineralBank_(base, 0);

    base.ApplyMineralSupport();

    // Zero-upkeep unit does not consume a free slot; one free slot covers `paid`.
    CHECK(base.GetHomeUnits().GetUnits().size() == 2);
    CHECK(base.GetResources().GetMineralBank() == 0);
}

TEST_CASE("Support 0 grants two free unit slots per base", "[unit][support][rating]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& a = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    Unit& b = fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    Unit& newest = fixture.MakeUnit(faction, 7, 4, {"test_chassis"}, &base);
    const UnitId_t newestId = newest.GetUnitId();

    LeaveMineralBank_(base, 0);
    base.ApplyMineralSupport();

    // Two free slots cover a+b; newest costs 1 with bank 0 → disbanded.
    REQUIRE(base.GetHomeUnits().GetUnits().size() == 2);
    CHECK(base.GetHomeUnits().GetUnits()[0] == &a);
    CHECK(base.GetHomeUnits().GetUnits()[1] == &b);
    CHECK(base.GetResources().GetMineralBank() == 0);
    CHECK_FALSE(UnitStillLive_(faction, newestId));
}

TEST_CASE("Facility free_unit_support stacks on Support 0", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Unit& oldest = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    Unit& middle = fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    Unit& newer = fixture.MakeUnit(faction, 7, 4, {"test_chassis"}, &base);
    Unit& newest = fixture.MakeUnit(faction, 8, 4, {"test_chassis"}, &base);
    Unit& fifth = fixture.MakeUnit(faction, 3, 4, {"test_chassis"}, &base);
    const UnitId_t fifthId = fifth.GetUnitId();

    base.GetBuildingManager().AddBuilding("free_support_depot"); // +2 → 4 free total
    LeaveMineralBank_(base, 0);

    base.ApplyMineralSupport();

    // Four free slots cover oldest…newest; fifth costs 1 with bank 0 → disbanded.
    REQUIRE(base.GetHomeUnits().GetUnits().size() == 4);
    CHECK(base.GetHomeUnits().GetUnits()[0] == &oldest);
    CHECK(base.GetHomeUnits().GetUnits()[1] == &middle);
    CHECK(base.GetHomeUnits().GetUnits()[2] == &newer);
    CHECK(base.GetHomeUnits().GetUnits()[3] == &newest);
    CHECK_FALSE(UnitStillLive_(faction, fifthId));
}

TEST_CASE("Support +2 grants four free unit slots", "[unit][support][rating]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("support_policy"));
    REQUIRE(base.GetEffectiveSocialRating(SocialRatingId_t::Support) == 2);

    for (int x = 5; x <= 8; ++x)
    {
        fixture.MakeUnit(faction, x, 4, {"test_chassis"}, &base);
    }
    Unit& fifth = fixture.MakeUnit(faction, 3, 4, {"test_chassis"}, &base);
    const UnitId_t fifthId = fifth.GetUnitId();

    LeaveMineralBank_(base, 0);
    base.ApplyMineralSupport();

    REQUIRE(base.GetHomeUnits().GetUnits().size() == 4);
    CHECK_FALSE(UnitStillLive_(faction, fifthId));
}

TEST_CASE("Support -2 grants one free unit slot", "[unit][support][rating]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("low_support_policy"));
    REQUIRE(base.GetEffectiveSocialRating(SocialRatingId_t::Support) == -2);

    Unit& free = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    Unit& paid = fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    const UnitId_t paidId = paid.GetUnitId();

    LeaveMineralBank_(base, 0);
    base.ApplyMineralSupport();

    REQUIRE(base.GetHomeUnits().GetUnits().size() == 1);
    CHECK(base.GetHomeUnits().GetUnits().front() == &free);
    CHECK_FALSE(UnitStillLive_(faction, paidId));
}

TEST_CASE("Support -4 doubles mineral upkeep and zeroes free slots",
          "[unit][support][rating]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("collapse_support_policy"));
    REQUIRE(base.GetEffectiveSocialRating(SocialRatingId_t::Support) == -4);

    Unit& unit = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    CHECK(unit.GetMineralUpkeep() == 2);

    LeaveMineralBank_(base, 2);
    base.ApplyMineralSupport();

    CHECK(base.GetHomeUnits().GetUnits().size() == 1);
    CHECK(base.GetResources().GetMineralBank() == 0);
}

TEST_CASE("FactionUnits mineral_upkeep Add increases support charge", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // Collapse free slots so the single unit is charged (Support 0 would cover it).
    faction.GetSocialEngineering().SetActivePolicy(
        fixture.socialPolicies().Get("collapse_support_policy"));

    Unit& unit = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    // Support -4 already adds +1 upkeep → chassis 1 + SE 1 = 2.
    CHECK(unit.GetMineralUpkeep() == 2);

    base.GetBuildingManager().AddBuilding("support_cost_penalty");
    CHECK(unit.GetMineralUpkeep() == 3);

    LeaveMineralBank_(base, 3);
    base.ApplyMineralSupport();

    CHECK(base.GetHomeUnits().GetUnits().size() == 1);
    CHECK(base.GetResources().GetMineralBank() == 0);
}

TEST_CASE("Insufficient minerals disband newest charged home units first", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // Support 0: two free. Third unit is charged.
    Unit& a = fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    Unit& b = fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    Unit& c = fixture.MakeUnit(faction, 7, 4, {"test_chassis"}, &base);
    const UnitId_t cId = c.GetUnitId();

    LeaveMineralBank_(base, 0);
    base.ApplyMineralSupport();

    REQUIRE(base.GetHomeUnits().GetUnits().size() == 2);
    CHECK(base.GetHomeUnits().GetUnits()[0] == &a);
    CHECK(base.GetHomeUnits().GetUnits()[1] == &b);
    CHECK(base.GetResources().GetMineralBank() == 0);
    CHECK_FALSE(UnitStillLive_(faction, cId));
}

TEST_CASE("Mineral support leaves remainder for production", "[unit][support]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    // Support 0 covers two; third costs 1 → remainder 4 from bank 5.
    fixture.MakeUnit(faction, 5, 4, {"test_chassis"}, &base);
    fixture.MakeUnit(faction, 6, 4, {"test_chassis"}, &base);
    fixture.MakeUnit(faction, 7, 4, {"test_chassis"}, &base);

    LeaveMineralBank_(base, 5);
    base.ApplyMineralSupport();

    CHECK(base.GetHomeUnits().GetUnits().size() == 3);
    CHECK(base.GetResources().GetMineralBank() == 4);
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
