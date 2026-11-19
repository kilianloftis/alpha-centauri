#include "GameFixtures.h"

#include "game/base/BaseManager.h"
#include "game/base/BaseTypes.h"
#include "game/buildings/BuildingManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/faction/Faction.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/PopulationManager.h"
#include "game/units/HealingConfig.h"
#include "game/units/HealingMutations.h"
#include "game/units/HealingRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitTurnLatches.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void FillLand_(WorldFixture& fixture)
{
    FillLand(fixture.map);
}

void RebuildTerritory_(FactionFixture& fixture)
{
    std::vector<const BaseManager*> bases;
    for (const Faction& rFaction : fixture.state.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            bases.push_back(&rBase);
        }
    }
    fixture.map.GetTerritory().Rebuild(fixture.map, bases);
}

std::vector<EffectConfig_t> LoadWorldRules_()
{
    return WorldRulesConfigParser::ParseConfig(FixturePath("world_rules.json"));
}

FactionFixture MakeHealingFixture_()
{
    return FactionFixture(k_TestMapWidth, k_TestMapHeight, LoadWorldRules_());
}

} // namespace

TEST_CASE("Idle damaged unit heals 10 percent of max HP", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    REQUIRE(ResolveStat(unit, StatId_t::HitPoints) == 10);

    unit.SetCurrentHp(1);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 2);
}

TEST_CASE("Heal-ineligible unit does not heal", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetCurrentHp(1);
    unit.MarkWasDefender();
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 1);
}

TEST_CASE("Spending move fragments marks heal ineligible", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SpendMoveFragments(1);
    CHECK(IsHealIneligible(unit));
}

TEST_CASE("Skip turn does not mark heal ineligible and still heals", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetCurrentHp(1);
    unit.SetOrder(SkipTurnOrder_t{});
    CHECK_FALSE(IsHealIneligible(unit));
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 2);
}

TEST_CASE("Field heal caps at 80 percent of max HP", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetCurrentHp(8);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 8);

    unit.SetCurrentHp(7);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 8);
}

TEST_CASE("Own territory adds heal rate via world_rules InOwnTerritory", "[healing]")
{
    FactionFixture fixture = MakeHealingFixture_();
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 5, 5);
    RebuildTerritory_(fixture);
    // Off the base square so Base's heal_rate does not stack (same tile parity as 5,5).
    Unit& unit = fixture.MakeUnit(faction, 7, 5, {"test_chassis"});
    REQUIRE(fixture.map.GetTerritory().GetOwner(unit.GetTile()) == faction.GetFactionId());
    unit.SetCurrentHp(1);
    ApplyIdleHeal(unit);
    // Seed 10% + own-territory 10%.
    CHECK(unit.GetCurrentHp() == 3);
}

TEST_CASE("Non-rioting base adds heal rate and raises the heal cap to 100 percent", "[healing]")
{
    FactionFixture fixture = MakeHealingFixture_();
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 5, 5);
    RebuildTerritory_(fixture);
    Unit& unit = fixture.MakeUnit(faction, 5, 5, {"test_chassis"}, &base);
    // Seed + Base heal_rate + own territory = 30% of max HP.
    unit.SetCurrentHp(1);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 4);

    unit.SetCurrentHp(8);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 10);
    CHECK(MaxHealableHp(unit) == 10);
}

TEST_CASE("Rioting base keeps the 100 percent heal cap but skips the base heal-rate bonus",
          "[healing]")
{
    FactionFixture fixture = MakeHealingFixture_();
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 5, 5);
    RebuildTerritory_(fixture);
    Unit& unit = fixture.MakeUnit(faction, 5, 5, {"test_chassis"}, &base);

    MoodState_t riot;
    riot.bRioting = true;
    riot.consecutiveRiotTurns = 1;
    base.GetPopulation().RestoreMoodState(riot);
    REQUIRE(base.GetPopulation().IsRioting());

    // Seed + own territory only (Base +10 canceled by riot_tiers heal_rate -10).
    unit.SetCurrentHp(1);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 3);

    unit.SetCurrentHp(8);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 10);
    CHECK(MaxHealableHp(unit) == 10);
}

TEST_CASE("Native on fungus gains heal rate and clears the field heal cap", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Tile& tile = *fixture.map.GetTile(8, 8);
    fixture.state.GetTileEffects().AddOccupantWithEffects(tile, std::string(ImprovementIds::k_Fungus));
    REQUIRE(tile.HasFeature(ImprovementIds::k_Fungus));

    Unit& native = fixture.MakeUnit(faction, 8, 8, {"test_chassis", "native_life_chassis"});
    REQUIRE(native.GetDesign().IsNativeLife());
    native.SetCurrentHp(1);
    ApplyIdleHeal(native);
    CHECK(native.GetCurrentHp() == 3);

    native.SetCurrentHp(8);
    ApplyIdleHeal(native);
    CHECK(native.GetCurrentHp() == 10);
    CHECK(MaxHealableHp(native) == 10);
}

TEST_CASE("Command Center fully repairs a land unit in a non-rioting base", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 5, 5);
    base.GetBuildingManager().AddBuilding("Command_Center");
    Unit& unit = fixture.MakeUnit(faction, 5, 5, {"test_chassis"}, &base);
    unit.SetCurrentHp(1);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 10);
}

TEST_CASE("Wrong-domain facility does not fully repair", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 5, 5);
    base.GetBuildingManager().AddBuilding("Naval_Yard");
    Unit& unit = fixture.MakeUnit(faction, 5, 5, {"test_chassis"}, &base);
    unit.SetCurrentHp(1);
    ApplyIdleHeal(unit);
    // Naval Yard only MinClamps sea heal_rate; land gets percent heal only.
    CHECK(unit.GetCurrentHp() > 1);
    CHECK(unit.GetCurrentHp() < 10);
}

TEST_CASE("HoldUntilHealed completes at the field heal cap", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetCurrentHp(8);
    unit.SetOrder(HoldUntilHealedOrder_t{});
    ApplyIdleHeal(unit);
    CHECK(unit.GetOrder().has_value());
    CHECK(fixture.state.GetUnitOrderExecutor().Execute(unit) == OrderProgress_t::Complete);
    CHECK_FALSE(unit.GetOrder().has_value());
}

TEST_CASE("Idle heal runs before BeginTurn rolls the turn-event ledger", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetCurrentHp(1);
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 2);
    unit.BeginTurn();
    CHECK_FALSE(IsHealIneligible(unit));
}

TEST_CASE("Heal-ineligible unit is skipped then BeginTurn rolls the ledger", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetCurrentHp(1);
    unit.MarkWasDefender();
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 1);
    unit.BeginTurn();
    CHECK_FALSE(IsHealIneligible(unit));
}

TEST_CASE("block_heal_on_was_defender false allows heal after WasDefender latch", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    fixture.state.GetGameData().healingConfig->blockHealOnWasDefender = false;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetCurrentHp(1);
    unit.MarkWasDefender();
    REQUIRE(unit.Latches().HasThisTurn(UnitTurnEvent_t::WasDefender));
    CHECK_FALSE(IsHealIneligible(unit));
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 2);
}

TEST_CASE("ignores_heal_block_on_was_attacker still heals after WasAttacker latch", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit =
        fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_ignores_heal_block_on_was_attacker"});
    unit.SetCurrentHp(1);
    unit.MarkWasAttacker();
    CHECK_FALSE(IsHealIneligible(unit));
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 2);
}

TEST_CASE("ignores_heal_block_on_was_attacker alone does not clear TookDamage block", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit =
        fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_ignores_heal_block_on_was_attacker"});
    unit.SetCurrentHp(2);
    unit.ApplyDamage(1);
    REQUIRE(unit.Latches().HasThisTurn(UnitTurnEvent_t::TookDamage));
    CHECK(IsHealIneligible(unit));
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 1);
}

TEST_CASE("ignores all heal blocks still heals after WasDefender latch", "[healing]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& faction = fixture.MakeFaction();
    Unit& unit =
        fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_ignores_all_heal_blocks"});
    unit.SetCurrentHp(1);
    unit.MarkWasDefender();
    CHECK_FALSE(IsHealIneligible(unit));
    ApplyIdleHeal(unit);
    CHECK(unit.GetCurrentHp() == 2);
}
