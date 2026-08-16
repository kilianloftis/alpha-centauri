// Facility energy upkeep: each owned building copy contributes BuildingConfig_t::upkeep;
// the faction sum is deducted from the energy treasury in the Upkeep stage.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/BuildingUpkeep.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace ac;
using namespace actest;

namespace
{

std::string WriteTempBuildings_(const std::string& rContents)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_building_upkeep_parser_test.json";
    std::ofstream out(path);
    out << rContents;
    return path.string();
}

} // namespace

TEST_CASE("BuildingConfigParser reads upkeep (default 0)", "[building][upkeep][parser]")
{
    BuildingRegistry registry;

    SECTION("omitted upkeep defaults to zero")
    {
        const std::string path = WriteTempBuildings_(R"([
            { "id": "free_hall", "name": "Free Hall", "mineral_cost": 5 }
        ])");
        REQUIRE_NOTHROW(registry.Load(path));
        const BuildingConfig_t* pConfig = registry.Find("free_hall");
        REQUIRE(pConfig != nullptr);
        CHECK(pConfig->upkeep == 0);
    }

    SECTION("explicit upkeep is stored")
    {
        const std::string path = WriteTempBuildings_(R"([
            { "id": "paid_hall", "name": "Paid Hall", "upkeep": 2 }
        ])");
        REQUIRE_NOTHROW(registry.Load(path));
        const BuildingConfig_t* pConfig = registry.Find("paid_hall");
        REQUIRE(pConfig != nullptr);
        CHECK(pConfig->upkeep == 2);
    }
}

TEST_CASE("Faction sums building upkeep across bases and copies", "[building][upkeep]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);

    CHECK(faction.GetBuildingUpkeep() == 0);

    baseA.GetBuildingManager().AddBuilding("upkeep_hall");
    CHECK(faction.GetBuildingUpkeep() == 2);

    // allow_multiple: each copy pays full upkeep.
    baseA.GetBuildingManager().AddBuilding("upkeep_hall");
    CHECK(faction.GetBuildingUpkeep() == 4);

    baseB.GetBuildingManager().AddBuilding("upkeep_hall");
    CHECK(faction.GetBuildingUpkeep() == 6);

    // Buildings with default upkeep 0 do not change the total.
    baseB.GetBuildingManager().AddBuilding("test_facility_a");
    CHECK(faction.GetBuildingUpkeep() == 6);
}

TEST_CASE("Building upkeep is available per type for UI", "[building][upkeep]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);

    baseA.GetBuildingManager().AddBuilding("upkeep_hall");
    baseA.GetBuildingManager().AddBuilding("upkeep_hall");
    baseA.GetBuildingManager().AddBuilding("test_facility_a");
    baseB.GetBuildingManager().AddBuilding("upkeep_hall");

    // Config accessor: maintenance for the building type itself.
    const BuildingConfig_t* pHall =
        baseA.GetBuildingManager().GetBuildings().front();
    REQUIRE(pHall != nullptr);
    CHECK(pHall->GetUpkeep() == 2);

    // Base panel: types owned at this base.
    const std::vector<BuildingUpkeepLine_t> baseLines = baseA.GetBuildingUpkeepByType();
    REQUIRE(baseLines.size() == 2);
    CHECK(baseLines[0].pConfig->id == "test_facility_a");
    CHECK(baseLines[0].count == 1);
    CHECK(baseLines[0].UpkeepPerCopy() == 0);
    CHECK(baseLines[0].TotalUpkeep() == 0);
    CHECK(baseLines[1].pConfig->id == "upkeep_hall");
    CHECK(baseLines[1].count == 2);
    CHECK(baseLines[1].UpkeepPerCopy() == 2);
    CHECK(baseLines[1].TotalUpkeep() == 4);
    CHECK(baseA.GetBuildingUpkeep() == 4);

    // Faction economy: same types rolled up across bases.
    const std::vector<BuildingUpkeepLine_t> factionLines = faction.GetBuildingUpkeepByType();
    REQUIRE(factionLines.size() == 2);
    CHECK(factionLines[0].pConfig->id == "test_facility_a");
    CHECK(factionLines[0].count == 1);
    CHECK(factionLines[0].TotalUpkeep() == 0);
    CHECK(factionLines[1].pConfig->id == "upkeep_hall");
    CHECK(factionLines[1].count == 3);
    CHECK(factionLines[1].TotalUpkeep() == 6);
    CHECK(SumBuildingUpkeep(factionLines) == faction.GetBuildingUpkeep());
}

TEST_CASE("ApplyBuildingUpkeep deducts from the faction energy treasury", "[building][upkeep]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    base.GetBuildingManager().AddBuilding("upkeep_hall");
    base.GetBuildingManager().AddBuilding("upkeep_hall");

    faction.GetEconomy().AddEnergy(10);
    CHECK(faction.GetBuildingUpkeep() == 4);

    faction.ApplyBuildingUpkeep();
    CHECK(faction.GetEconomy().GetEnergy() == 6);

    // Zero upkeep is a no-op even with an empty treasury.
    Faction& empty = fixture.MakeFaction();
    CHECK(empty.GetBuildingUpkeep() == 0);
    CHECK_NOTHROW(empty.ApplyBuildingUpkeep());
    CHECK(empty.GetEconomy().GetEnergy() == 0);
}

TEST_CASE("ApplyBuildingUpkeep throws when the treasury cannot cover upkeep",
          "[building][upkeep]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    base.GetBuildingManager().AddBuilding("upkeep_hall");

    faction.GetEconomy().AddEnergy(1);
    CHECK_THROWS_AS(faction.ApplyBuildingUpkeep(), std::runtime_error);
    CHECK(faction.GetEconomy().GetEnergy() == 1);
}

TEST_CASE("GetNetIncomePerTurn subtracts building upkeep", "[building][upkeep]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    const int before = faction.GetNetIncomePerTurn();
    base.GetBuildingManager().AddBuilding("upkeep_hall");
    CHECK(faction.GetNetIncomePerTurn() == before - 2);
}

TEST_CASE("Headquarters does not charge facility energy upkeep", "[building][upkeep]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    CHECK(fixture.buildings().Get("Headquarters").upkeep == 0);
    base.GetBuildingManager().AddBuilding("Headquarters");
    CHECK(faction.GetBuildingUpkeep() == 0);
}

TEST_CASE("Stock Headquarters charges no energy upkeep", "[building][upkeep]")
{
    BuildingRegistry registry;
    registry.Load(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/buildings/buildings.json");
    CHECK(registry.Get("Headquarters").upkeep == 0);
}

TEST_CASE("Continuous GrantBuilding targets do not pay maintenance", "[building][upkeep][grant]")
{
    // Command Nexus-style grants expand the target's continuous effects without constructing
    // a copy. Upkeep only tallies BuildingManager holdings, so the virtual grant is free.
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    base.GetBuildingManager().AddBuilding("grantor_local");
    CHECK_FALSE(base.GetBuildingManager().HasBuilding("granted_hall"));
    CHECK(faction.GetBuildingUpkeep() == 0);

    // grantor_local itself has default upkeep 0; the only non-zero type in fixtures here
    // is granted_hall (upkeep 5), which must not appear while only virtually granted.
    for (const BuildingUpkeepLine_t& rLine : faction.GetBuildingUpkeepByType())
    {
        CHECK(rLine.pConfig->id != "granted_hall");
    }

    // A real constructed copy of the same type does pay.
    base.GetBuildingManager().AddBuilding("granted_hall");
    CHECK(faction.GetBuildingUpkeep() == 5);
}

TEST_CASE("FacilityEnergyUpkeep modifiers apply via buildingFilter", "[building][upkeep]")
{
    FactionFixture fixture;
    const BuildingConfig_t& rHall = fixture.dataContext.buildingRegistry->Get("upkeep_hall");
    const BuildingConfig_t& rGranted = fixture.dataContext.buildingRegistry->Get("granted_hall");

    EffectPool pool;
    const EffectConfig_t& rAll = pool.StatMod(
        StatId_t::FacilityEnergyUpkeep, -50.0, ModifierOp_t::AddPercent,
        EffectScope_t::FactionGlobal, std::nullopt, std::nullopt,
        EffectPersistence_t::Continuous, std::nullopt,
        BuildingFilterAll_t{});
    const EffectConfig_t& rId = pool.StatMod(
        StatId_t::FacilityEnergyUpkeep, -50.0, ModifierOp_t::AddPercent,
        EffectScope_t::FactionGlobal, std::nullopt, std::nullopt,
        EffectPersistence_t::Continuous, std::nullopt,
        BuildingFilterId_t{"upkeep_hall"});
    const EffectConfig_t& rCat = pool.StatMod(
        StatId_t::FacilityEnergyUpkeep, -50.0, ModifierOp_t::AddPercent,
        EffectScope_t::FactionGlobal, std::nullopt, std::nullopt,
        EffectPersistence_t::Continuous, std::nullopt,
        BuildingFilterCategory_t{GameCategory_t::Discover});

    ActiveEffect_t allFx{rAll, "test"};
    ActiveEffect_t idFx{rId, "test"};
    ActiveEffect_t catFx{rCat, "test"};

    CHECK(ResolveFacilityEnergyUpkeepPerCopy(rHall, {}) == 2);
    CHECK(ResolveFacilityEnergyUpkeepPerCopy(rHall, std::vector{allFx}) == 1);
    CHECK(ResolveFacilityEnergyUpkeepPerCopy(rHall, std::vector{idFx}) == 1);
    CHECK(ResolveFacilityEnergyUpkeepPerCopy(rGranted, std::vector{idFx}) == 5);
    // Discover category −50% on granted_hall (upkeep 5): 5 × 0.5 → lround 3
    CHECK(ResolveFacilityEnergyUpkeepPerCopy(rGranted, std::vector{catFx}) == 3);
    CHECK(ResolveFacilityEnergyUpkeepPerCopy(rHall, std::vector{catFx}) == 2);
}

TEST_CASE("Discovered tech FacilityEnergyUpkeep reduces maintenance", "[building][upkeep][tech]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    base.GetBuildingManager().AddBuilding("upkeep_hall");
    base.GetBuildingManager().AddBuilding("granted_hall");

    CHECK(faction.GetBuildingUpkeep() == 7); // 2 + 5

    faction.GetResearch().AddDiscoveredTech("efficient_maintenance");
    // -50% on upkeep_hall only → 1 + 5
    CHECK(faction.GetBuildingUpkeep() == 6);
    const std::vector<BuildingUpkeepLine_t> lines = faction.GetBuildingUpkeepByType();
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].pConfig->id == "granted_hall");
    CHECK(lines[0].UpkeepPerCopy() == 5);
    CHECK(lines[1].pConfig->id == "upkeep_hall");
    CHECK(lines[1].UpkeepPerCopy() == 1);
}
