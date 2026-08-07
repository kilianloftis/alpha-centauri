// Package 9 — invariants enforced at the single mutation point (BuildingManager::AddBuilding)
// rather than only where the build menu is generated, plus the one tombstone rule that every
// destruction path shares.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace ac;
using namespace actest;

namespace
{

// Written outside the source tree: a fixture-directory temp file dirties git on every run and
// risks contaminating anything that loads that directory wholesale.
std::string WriteTempBuildings_(const std::string& rContents)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_building_parser_test.json";
    std::ofstream out(path);
    out << rContents;
    return path.string();
}


// A session with two factions, so "some other faction already built it" is expressible.
struct BuildingGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    BuildingGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, k_TestRngSeed);

        pPlayer = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
        pAi = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed + 1));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        BaseManager* pBase = rFaction.CreateBase(
            pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(x, y),
            fixtures.dataContext, pState->GetTileEffects(),
            pState->GetSecretProjectAvailability());
        REQUIRE(pBase != nullptr);
        return *pBase;
    }
};

} // namespace

TEST_CASE("A secret project cannot be built twice, in any base of any faction",
          "[building][secret-project]")
{
    // Uniqueness used to be checked only when the build menu was generated. Two bases that both
    // had the project listed when they queued it — same faction, or two factions in one
    // BaseProduction pass — would both complete it, because the path that actually grants a
    // building (ProductionManager -> OnProductionCompleted -> AddBuilding) checked nothing.
    BuildingGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 2, 2);
    BaseManager& playerSecond = game.MakeBase(*game.pPlayer, 6, 2);
    BaseManager& aiBase = game.MakeBase(*game.pAi, 6, 6);

    playerBase.GetBuildingManager().AddBuilding("test_secret_project");

    // Not again in another base of the same faction...
    CHECK_THROWS_AS(playerSecond.GetBuildingManager().AddBuilding("test_secret_project"),
                    std::runtime_error);
    // ...nor in a rival's base.
    CHECK_THROWS_AS(aiBase.GetBuildingManager().AddBuilding("test_secret_project"),
                    std::runtime_error);
    // ...nor a second time in the base that owns it.
    CHECK_THROWS_AS(playerBase.GetBuildingManager().AddBuilding("test_secret_project"),
                    std::runtime_error);

    CHECK(game.pPlayer->CountBuildings("test_secret_project") == 1);
    CHECK(game.pAi->CountBuildings("test_secret_project") == 0);
}

TEST_CASE("A non-allowMultiple building cannot be duplicated in one base", "[building]")
{
    // allowMultiple is the config field that says whether stacking is legal; nothing enforced
    // it at the point that adds a building.
    BuildingGame_ game;
    BaseManager& base = game.MakeBase(*game.pPlayer, 2, 2);

    base.GetBuildingManager().AddBuilding("Command_Center");
    CHECK_THROWS_AS(base.GetBuildingManager().AddBuilding("Command_Center"), std::runtime_error);
    CHECK(game.pPlayer->CountBuildings("Command_Center") == 1);

    // A building that declares allow_multiple still stacks.
    CHECK_NOTHROW(base.GetBuildingManager().AddBuilding("test_facility_a"));
    CHECK_NOTHROW(base.GetBuildingManager().AddBuilding("test_facility_a"));
    CHECK(game.pPlayer->CountBuildings("test_facility_a") == 2);
}

TEST_CASE("A destroyed secret project stays unavailable but is owned by nobody",
          "[building][secret-project]")
{
    // IsCompleted answered both questions with one method, so a razed project read as somebody's
    // — wrong for any caller that wants ownership (a UI label, a victory check, diplomacy).
    BuildingGame_ game;
    BaseManager& base = game.MakeBase(*game.pPlayer, 2, 2);
    const SecretProjectAvailabilityCalculator& rAvailability =
        game.pState->GetSecretProjectAvailability();

    CHECK_FALSE(rAvailability.IsUnavailable("test_secret_project"));
    CHECK_FALSE(rAvailability.IsOwnedByAnyFaction("test_secret_project"));

    base.GetBuildingManager().AddBuilding("test_secret_project");
    CHECK(rAvailability.IsUnavailable("test_secret_project"));
    CHECK(rAvailability.IsOwnedByAnyFaction("test_secret_project"));

    // Destroying the copy tombstones it: still unavailable, but no longer owned.
    base.GetBuildingManager().DestroyBuilding("test_secret_project");
    game.pState->MarkSecretProjectDestroyed("test_secret_project");
    CHECK(rAvailability.IsUnavailable("test_secret_project"));
    CHECK_FALSE(rAvailability.IsOwnedByAnyFaction("test_secret_project"));

    // And it cannot be rebuilt, by anyone.
    BaseManager& aiBase = game.MakeBase(*game.pAi, 6, 6);
    CHECK_THROWS_AS(aiBase.GetBuildingManager().AddBuilding("test_secret_project"),
                    std::runtime_error);
}

TEST_CASE("Losing a secret-project race drops the item instead of killing the turn",
          "[building][secret-project][production]")
{
    // The player can queue the same project in two of their own bases — both are offered it,
    // and nothing revokes an already-queued item when one completes. Completing the second must
    // not throw: nothing catches between here and main().
    BuildingGame_ game;
    BaseManager& winner = game.MakeBase(*game.pPlayer, 2, 2);
    BaseManager& loser = game.MakeBase(*game.pPlayer, 6, 2);

    winner.GetBuildingManager().AddBuilding("test_secret_project");
    REQUIRE_FALSE(loser.GetBuildingManager().CanAddBuilding("test_secret_project"));

    const BuildingConfig_t& rProject =
        game.fixtures.dataContext.buildingRegistry->Get("test_secret_project");
    loser.GetProduction().SetProduction(&rProject);
    loser.GetProduction().SetMineralStockpile(10000); // far past any cost, so it completes now

    CHECK_NOTHROW(loser.ApplyProduction());
    CHECK(game.pPlayer->CountBuildings("test_secret_project") == 1);
    CHECK_FALSE(loser.GetBuildingManager().HasBuilding("test_secret_project"));
}

TEST_CASE("The building parser rejects rather than silently defaulting", "[building][config]")
{
    // json::value() substitutes the default for a key of the wrong shape, so "allow_multiple":
    // "yes" parsed as false and a typo'd key was accepted and ignored — the modder's setting
    // silently never applied.
    BuildingRegistry registry;

    SECTION("a wrong-typed value names the building and the key")
    {
        const std::string path = WriteTempBuildings_(R"([
            { "id": "bad_bool", "name": "Bad", "allow_multiple": "yes" }
        ])");
        CHECK_THROWS_WITH(registry.Load(path),
                          Catch::Matchers::ContainsSubstring("bad_bool")
                              && Catch::Matchers::ContainsSubstring("allow_multiple"));
    }

    SECTION("an unknown key is rejected, not ignored")
    {
        const std::string path = WriteTempBuildings_(R"([
            { "id": "typo", "name": "Typo", "allow_multiples": true }
        ])");
        CHECK_THROWS_WITH(registry.Load(path),
                          Catch::Matchers::ContainsSubstring("allow_multiples"));
    }

    SECTION("category is optional, since nothing reads it")
    {
        const std::string path = WriteTempBuildings_(R"([
            { "id": "no_category", "name": "No Category" }
        ])");
        CHECK_NOTHROW(registry.Load(path));
    }
}

TEST_CASE("BuildingRegistry validates the whole set", "[building][config]")
{
    // The base class hosts a whole-set validation hook that BuildingRegistry inherited and
    // never used, so cross-entry contradictions surfaced at runtime instead of at load.
    BuildingRegistry registry;

    SECTION("a stackable secret project is a contradiction")
    {
        const std::string path = WriteTempBuildings_(R"([
            { "id": "stacky", "name": "Stacky", "secret_project": true, "allow_multiple": true }
        ])");
        CHECK_THROWS_WITH(registry.Load(path),
                          Catch::Matchers::ContainsSubstring("mutually exclusive"));
    }

    SECTION("a negative mineral cost is rejected")
    {
        const std::string path = WriteTempBuildings_(R"([
            { "id": "cheap", "name": "Cheap", "mineral_cost": -5 }
        ])");
        CHECK_THROWS_WITH(registry.Load(path),
                          Catch::Matchers::ContainsSubstring("mineral_cost"));
    }
}
