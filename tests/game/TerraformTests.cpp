#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/TerraformRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

struct TerraformGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;

    TerraformGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
            pTile->SetRockiness(Rockiness_t::Flat);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules, actest::k_TestRngSeed);

        auto pFaction = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext, pState->GetWorldMap(), fixtures.settings,
            actest::k_TestFactionSeed);
        pPlayer = &pState->AddFaction(std::move(pFaction));
        pPlayer->GetEconomy().AddEnergy(100);
    }

    BaseManager& MakeBase(int x, int y)
    {
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = pPlayer->CreateBase(
            pState->AllocateBaseId(), "TestBase", pTile, fixtures.dataContext,
            pState->GetTileEffects(), pState->GetSecretProjectAvailability());
        REQUIRE(pBase);
        return *pBase;
    }

    Unit& MakeFormer(int x, int y, BaseManager* pHome = nullptr)
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        int slotIndex = 0;
        for (const std::string& rId : {"test_chassis", "test_terraformer"})
        {
            const UnitComponentConfig_t* pComponent = fixtures.unitComponents.Find(rId);
            REQUIRE(pComponent);
            UnitSlotConfig_t slot;
            slot.id = "slot_" + std::to_string(slotIndex++);
            slot.displayName = slot.id;
            slot.componentType = pComponent->type;
            slots.push_back(slot);
            assigned[slot.id] = pComponent;
        }
        fixtures.designs.emplace_back(slots, assigned);
        UnitDesign& rDesign = fixtures.designs.back();
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return pPlayer->GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), rDesign, pState->GetWorldMap().GetUnitPositions(), *pTile,
            pHome);
    }

    void FinishTerraform(Unit& rUnit)
    {
        UnitOrderExecutor& rExec = pState->GetUnitOrderExecutor();
        while (rUnit.GetOrder().has_value()
               && std::holds_alternative<TerraformOrder_t>(*rUnit.GetOrder()))
        {
            REQUIRE(rExec.Execute(rUnit) != OrderProgress_t::Expended);
        }
    }
};

} // namespace

TEST_CASE("TryStartTerraform places Road after turns complete", "[unit][terraform]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home);
    REQUIRE(former.GetFlag(RuleFlagId_t::Terraform));

    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);
    REQUIRE_FALSE(tile.HasImprovement("Road"));

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "Road", *game.pState));
    REQUIRE(former.GetOrder().has_value());
    CHECK(std::get<TerraformOrder_t>(*former.GetOrder()).improvementId == "Road");
    CHECK(former.GetMoveFragmentsRemaining() == 0);

    game.FinishTerraform(former);
    CHECK(tile.HasImprovement("Road"));
    CHECK_FALSE(former.GetOrder().has_value());
}

TEST_CASE("TryStartTerraform rejects non-formers and exclusions", "[unit][terraform]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(4, 4);

    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    const UnitComponentConfig_t* pChassis = game.fixtures.unitComponents.Find("test_chassis");
    REQUIRE(pChassis);
    UnitSlotConfig_t slot;
    slot.id = "slot_0";
    slot.displayName = slot.id;
    slot.componentType = pChassis->type;
    slots.push_back(slot);
    assigned[slot.id] = pChassis;
    game.fixtures.designs.emplace_back(slots, assigned);
    Tile* pTile = game.pState->GetWorldMap().GetTile(6, 4);
    Unit& scout = game.pPlayer->GetUnitManager().CreateUnit(
        game.pState->AllocateUnitId(), game.fixtures.designs.back(),
        game.pState->GetWorldMap().GetUnitPositions(), *pTile,
        &home);

    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStartTerraform(scout, "Farm", *game.pState));

    Unit& former = game.MakeFormer(7, 4, &home);
    Tile& rocky = *game.pState->GetWorldMap().GetTile(7, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "Farm", *game.pState));
}

TEST_CASE("Terraform mutations: level, fungus, aquifer", "[unit][terraform][mutate]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home);
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);
    tile.SetRockiness(Rockiness_t::Rocky);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "LevelTerrain", *game.pState));
    game.FinishTerraform(former);
    CHECK(tile.GetRockiness() == Rockiness_t::Rolling);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "PlantFungus", *game.pState));
    game.FinishTerraform(former);
    CHECK(tile.GetHasFungus());

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "RemoveFungus", *game.pState));
    game.FinishTerraform(former);
    CHECK_FALSE(tile.GetHasFungus());

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "Aquifer", *game.pState));
    game.FinishTerraform(former);
    CHECK(tile.GetHasAquifer());
    CHECK(tile.GetHasRiver());
}

TEST_CASE("Raise and lower land change elevation", "[unit][terraform][mutate]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home);
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);
    tile.SetElevation(1000);

    const int energyBefore = game.pPlayer->GetEconomy().GetEnergy();
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "RaiseLand", *game.pState));
    CHECK(game.pPlayer->GetEconomy().GetEnergy() < energyBefore);
    game.FinishTerraform(former);
    CHECK(tile.GetElevation() == 2000);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "LowerLand", *game.pState));
    game.FinishTerraform(former);
    CHECK(tile.GetElevation() == 1000);
}

TEST_CASE("Lowering land stops at Planet's floor instead of throwing",
          "[unit][terraform][mutate]")
{
    // A sea Former's LowerLand gate was unconditional, so on a deep-ocean tile the mutation
    // drove elevation past the map's lower bound. Tile::SetElevation now rejects that, which
    // would throw out of order execution - nothing catches between there and main().
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    Unit& seaFormer =
        fixture.MakeUnit(faction, 6, 4, {"test_sea_chassis", "test_terraformer"}, &base);
    Tile& tile = fixture.At(6, 4);
    tile.SetElevation(k_MinElevation + 500);

    const ImprovementConfig_t& rLowerLand = fixture.improvements.Get("LowerLand");
    CHECK_NOTHROW(ApplyTerraformResult(tile, rLowerLand, *fixture.ctx, fixture.map, seaFormer));
    CHECK(tile.GetElevation() == k_MinElevation + 500);

    // One more step of headroom and it still applies.
    tile.SetElevation(k_MinElevation + 1000);
    CHECK(ApplyTerraformResult(tile, rLowerLand, *fixture.ctx, fixture.map, seaFormer));
    CHECK(tile.GetElevation() == k_MinElevation);
}

TEST_CASE("ApplyTerraformResult places Farm via rules helper", "[unit][terraform]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    Unit& former = fixture.MakeUnit(faction, 6, 4, {"test_chassis", "test_terraformer"}, &base);
    Tile& tile = fixture.At(6, 4);
    tile.SetElevation(100);
    tile.SetRockiness(Rockiness_t::Flat);

    const ImprovementConfig_t* pFarm = fixture.improvements.Find("Farm");
    REQUIRE(pFarm);
    REQUIRE(ApplyTerraformResult(tile, *pFarm, *fixture.ctx, fixture.map, former));
    CHECK(tile.HasImprovement("Farm"));
}
