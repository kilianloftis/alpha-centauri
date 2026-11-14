#include "GameFixtures.h"
#include "TestTempDir.h"

#include "game/base/BaseManager.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingManager.h"
#include "game/buildings/BuildingPrerequisiteValidator.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/BuildingRules.h"
#include "game/buildings/RequiredBuildings.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/map/WorldMap.h"
#include "game/production/IConstructable.h"
#include "game/production/ProductionManager.h"
#include "game/production/ProductionRules.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

std::filesystem::path WriteTempBuildingJson(const std::string& name, const std::string& body)
{
    const std::filesystem::path path = TestTempDir::Path() / name;
    std::ofstream out(path);
    out << "[\n" << body << "\n]\n";
    return path;
}

struct PrereqGame_
{
    FactionFixture fixtures;
    Faction* pFaction = nullptr;

    PrereqGame_()
    {
        FillLand(fixtures.map);
        pFaction = &fixtures.state.AddFaction(std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, fixtures.state.GetUnitIdAllocator(),
            k_TestFactionSeed));
    }

    BaseManager& MakeBase()
    {
        BaseManager* pBase = pFaction->CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", fixtures.state.GetWorldMap().GetTile(8, 8),
            fixtures.state.GetTileEffects(), 3);
        REQUIRE(pBase != nullptr);
        return *pBase;
    }

    const BuildingConfig_t& Building(const std::string& rId) const
    {
        return fixtures.buildings().Get(rId);
    }
};

bool ConstructableContains_(const BaseManager& rBase, const BuildingConfig_t& rBuilding)
{
    const std::vector<const IConstructable*> items = ConstructableItems(rBase);
    return std::ranges::contains(items, static_cast<const IConstructable*>(&rBuilding));
}

} // namespace

TEST_CASE("BuildingConfigParser reads required_buildings entries",
          "[buildings][prereq][parser]")
{
    const std::filesystem::path path = WriteTempBuildingJson(
        "ac_required_buildings_parse.json",
        R"(
  {
    "id": "gated",
    "name": "Gated",
    "mineral_cost": 10,
    "category": "grow",
    "required_buildings": {
      "any": [{ "building": "a", "scope": "ThisBase" }],
      "all": [
        { "building": "b", "scope": "ThisFaction", "count": 2 },
        { "building": "c", "scope": "World" }
      ],
      "none": [{ "building": "d", "scope": "ThisBase" }]
    }
  }
)");

    const std::vector<BuildingConfig_t> configs = BuildingConfigParser::ParseConfig(path.string());
    REQUIRE(configs.size() == 1);
    const RequiredBuildings_t& rRequired = configs[0].requiredBuildings;
    REQUIRE(rRequired.any.size() == 1);
    CHECK(rRequired.any[0].building == "a");
    CHECK(rRequired.any[0].scope == BuildingRequirementScope_t::ThisBase);
    CHECK(rRequired.any[0].count == 1);
    REQUIRE(rRequired.all.size() == 2);
    CHECK(rRequired.all[0].building == "b");
    CHECK(rRequired.all[0].scope == BuildingRequirementScope_t::ThisFaction);
    CHECK(rRequired.all[0].count == 2);
    CHECK(rRequired.all[1].building == "c");
    CHECK(rRequired.all[1].scope == BuildingRequirementScope_t::World);
    REQUIRE(rRequired.none.size() == 1);
    CHECK(rRequired.none[0].building == "d");
    std::filesystem::remove(path);
}

TEST_CASE("BuildingConfigParser treats omitted required_buildings as empty",
          "[buildings][prereq][parser]")
{
    const std::filesystem::path path = WriteTempBuildingJson(
        "ac_required_buildings_omit.json",
        R"(
  {
    "id": "plain",
    "name": "Plain",
    "mineral_cost": 5,
    "category": "grow"
  }
)");

    const std::vector<BuildingConfig_t> configs = BuildingConfigParser::ParseConfig(path.string());
    REQUIRE(configs.size() == 1);
    CHECK(configs[0].requiredBuildings.any.empty());
    CHECK(configs[0].requiredBuildings.all.empty());
    CHECK(configs[0].requiredBuildings.none.empty());
    std::filesystem::remove(path);
}

TEST_CASE("BuildingConfigParser requires scope on required_buildings entries",
          "[buildings][prereq][parser]")
{
    const std::filesystem::path path = WriteTempBuildingJson(
        "ac_required_buildings_no_scope.json",
        R"(
  {
    "id": "gated",
    "name": "Gated",
    "category": "grow",
    "required_buildings": {
      "any": [{ "building": "a" }]
    }
  }
)");
    CHECK_THROWS_AS(BuildingConfigParser::ParseConfig(path.string()), std::runtime_error);
    std::filesystem::remove(path);
}

TEST_CASE("ValidateBuildingPrerequisiteReferences rejects unknown and self ids",
          "[buildings][prereq][validation]")
{
    GameDataContext data;
    data.buildingRegistry = std::make_unique<BuildingRegistry>();

    SECTION("unknown any id")
    {
        const std::filesystem::path path = WriteTempBuildingJson(
            "ac_required_buildings_unknown.json",
            R"(
  { "id": "gated", "name": "Gated", "category": "grow",
    "required_buildings": { "any": [{ "building": "missing", "scope": "ThisBase" }] } }
)");
        data.buildingRegistry->Load(path.string());
        CHECK_THROWS_WITH(ValidateBuildingPrerequisiteReferences(data),
                          Catch::Matchers::ContainsSubstring("unknown building"));
        std::filesystem::remove(path);
    }

    SECTION("self id in all")
    {
        const std::filesystem::path path = WriteTempBuildingJson(
            "ac_required_buildings_self.json",
            R"(
  { "id": "gated", "name": "Gated", "category": "grow",
    "required_buildings": { "all": [{ "building": "gated", "scope": "ThisBase" }] } }
)");
        data.buildingRegistry->Load(path.string());
        CHECK_THROWS_WITH(ValidateBuildingPrerequisiteReferences(data),
                          Catch::Matchers::ContainsSubstring("must not include itself"));
        std::filesystem::remove(path);
    }

    SECTION("known ids pass")
    {
        const std::filesystem::path path = WriteTempBuildingJson(
            "ac_required_buildings_ok.json",
            R"(
  { "id": "base", "name": "Base", "category": "grow" },
  { "id": "gated", "name": "Gated", "category": "grow",
    "required_buildings": { "any": [{ "building": "base", "scope": "ThisBase" }] } }
)");
        data.buildingRegistry->Load(path.string());
        CHECK_NOTHROW(ValidateBuildingPrerequisiteReferences(data));
        std::filesystem::remove(path);
    }
}

TEST_CASE("MeetsBuildingProductionPrereqs any / all / none",
          "[buildings][prereq][rules]")
{
    PrereqGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rNeedsAny = game.Building("test_prereq_needs_any");
    const BuildingConfig_t& rNeedsAll = game.Building("test_prereq_needs_all");
    const BuildingConfig_t& rForbid = game.Building("test_prereq_forbid");

    CHECK_FALSE(MeetsBuildingProductionPrereqs(base, rNeedsAny, BuildingPresenceMode_t::BuiltOnly));
    base.GetBuildingManager().AddBuilding("test_prereq_base");
    CHECK(MeetsBuildingProductionPrereqs(base, rNeedsAny, BuildingPresenceMode_t::BuiltOnly));

    CHECK_FALSE(MeetsBuildingProductionPrereqs(base, rNeedsAll, BuildingPresenceMode_t::BuiltOnly));
    base.GetBuildingManager().AddBuilding("test_facility_a");
    CHECK_FALSE(MeetsBuildingProductionPrereqs(base, rNeedsAll, BuildingPresenceMode_t::BuiltOnly));
    base.GetBuildingManager().AddBuilding("test_facility_b");
    CHECK(MeetsBuildingProductionPrereqs(base, rNeedsAll, BuildingPresenceMode_t::BuiltOnly));

    CHECK_FALSE(MeetsBuildingProductionPrereqs(base, rForbid, BuildingPresenceMode_t::BuiltOnly));
    base.GetBuildingManager().DestroyBuilding("test_facility_a");
    CHECK(MeetsBuildingProductionPrereqs(base, rForbid, BuildingPresenceMode_t::BuiltOnly));
}

TEST_CASE("MeetsBuildingProductionPrereqs ThisFaction scope for secret projects",
          "[buildings][prereq][rules][sp]")
{
    PrereqGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rOrSp = game.Building("test_prereq_or_sp");

    CHECK_FALSE(MeetsBuildingProductionPrereqs(base, rOrSp, BuildingPresenceMode_t::BuiltOnly));
    base.GetBuildingManager().AddBuilding("test_secret_project");
    CHECK(MeetsBuildingProductionPrereqs(base, rOrSp, BuildingPresenceMode_t::BuiltOnly));
}

TEST_CASE("ThisBase BuiltOrQueued counts current and queued; BuiltOnly does not",
          "[buildings][prereq][rules][queue]")
{
    PrereqGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rBase = game.Building("test_prereq_base");
    const BuildingConfig_t& rNeedsAny = game.Building("test_prereq_needs_any");
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rBase, rEffects);
    CHECK(MeetsBuildingProductionPrereqs(base, rNeedsAny, BuildingPresenceMode_t::BuiltOrQueued));
    CHECK_FALSE(MeetsBuildingProductionPrereqs(base, rNeedsAny, BuildingPresenceMode_t::BuiltOnly));

    rProduction.SetProduction(nullptr, rEffects);
    rProduction.Insert(0, rBase);
    CHECK(MeetsBuildingProductionPrereqs(base, rNeedsAny, BuildingPresenceMode_t::BuiltOrQueued));
    CHECK_FALSE(MeetsBuildingProductionPrereqs(base, rNeedsAny, BuildingPresenceMode_t::BuiltOnly));
}

TEST_CASE("ConstructableItems lists Hab Dome only when Hab Complex is built or queued",
          "[buildings][prereq][production]")
{
    PrereqGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rComplex = game.Building("Hab_Complex");
    const BuildingConfig_t& rDome = game.Building("Habitation_Dome");

    CHECK_FALSE(ConstructableContains_(base, rDome));

    base.GetBuildingManager().AddBuilding("Hab_Complex");
    CHECK(ConstructableContains_(base, rDome));
    base.GetBuildingManager().DestroyBuilding("Hab_Complex");
    CHECK_FALSE(ConstructableContains_(base, rDome));

    base.GetProduction().SetProduction(&rComplex, base.GetBaseEffects());
    CHECK(ConstructableContains_(base, rDome));
}

TEST_CASE("Destroying a prerequisite leaves current production completable",
          "[buildings][prereq][production][mid-build]")
{
    PrereqGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rDome = game.Building("Habitation_Dome");
    ProductionManager& rProduction = base.GetProduction();

    base.GetBuildingManager().AddBuilding("Hab_Complex");
    rProduction.SetProduction(&rDome, base.GetBaseEffects());
    rProduction.SetMineralStockpile(ItemMineralCost(base, rDome));
    base.GetBuildingManager().DestroyBuilding("Hab_Complex");

    CHECK(CanRetainCurrentProduction(base, rDome));
    CHECK(rProduction.IsReadyToComplete());
    const std::optional<std::string> dropped = rProduction.ValidateQueue();
    CHECK_FALSE(dropped.has_value());
    REQUIRE(rProduction.GetCurrentProduction() != nullptr);
    CHECK(rProduction.GetCurrentProduction()->GetId() == rDome.id);
}

TEST_CASE("Promote skips a queue head that fails BuiltOnly facility prereqs",
          "[buildings][prereq][production][promote]")
{
    PrereqGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rNeedsAny = game.Building("test_prereq_needs_any");
    const BuildingConfig_t& rBasePrereq = game.Building("test_prereq_base");
    const BuildingConfig_t& rB = game.Building("test_facility_b");
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    base.GetBuildingManager().AddBuilding("test_facility_b");
    rProduction.SetProduction(&rB, rEffects);
    rProduction.Insert(0, rNeedsAny);
    rProduction.Insert(1, rBasePrereq);

    const std::optional<std::string> dropped = rProduction.ValidateQueue();
    REQUIRE(dropped.has_value());
    REQUIRE(rProduction.GetCurrentProduction() != nullptr);
    CHECK(rProduction.GetCurrentProduction()->GetId() == rBasePrereq.id);
    CHECK_FALSE(std::ranges::contains(rProduction.GetQueue(),
                                      static_cast<const IConstructable*>(&rNeedsAny)));
}

TEST_CASE("Upcoming ValidateQueue drops a building when its prereq is fully gone",
          "[buildings][prereq][production][upcoming]")
{
    PrereqGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rNeedsAny = game.Building("test_prereq_needs_any");
    const BuildingConfig_t& rA = game.Building("test_facility_a");
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    base.GetBuildingManager().AddBuilding("test_prereq_base");
    rProduction.SetProduction(&rA, rEffects);
    rProduction.Insert(0, rNeedsAny);
    REQUIRE(std::ranges::contains(rProduction.GetQueue(),
                                  static_cast<const IConstructable*>(&rNeedsAny)));

    base.GetBuildingManager().DestroyBuilding("test_prereq_base");
    rProduction.ValidateQueue();
    CHECK_FALSE(std::ranges::contains(rProduction.GetQueue(),
                                      static_cast<const IConstructable*>(&rNeedsAny)));
}
