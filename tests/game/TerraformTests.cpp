#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/map/ElevationRulesConfig.h"
#include "game/units/TerraformRules.h"
#include "game/units/Unit.h"
#include "game/effects/EffectConfig.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <memory>
#include <random>
#include <string>
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

    explicit TerraformGame_(bool bExcludes = false, const ElevationRulesConfig_t* pRules = nullptr)
        : fixtures(9, 9, {}, bExcludes ? k_ExcludesOccupantFiles : OccupantFiles_t{})
    {
        auto pMap = std::make_unique<WorldMap>(9, 9, pRules ? *pRules : actest::TestMapRules());
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
            pTile->SetRockiness(Rockiness_t::Flat);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.dataContext, settings, actest::k_TestRngSeed);
        pState->GetUnitOrderExecutor().SetGameDataContext(fixtures.dataContext);

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

    Unit& MakeFormer(int x, int y, BaseManager* pHome = nullptr,
                     const UnitComponentConfig_t* pTerraformer = nullptr,
                     const std::string& rChassis = "test_chassis")
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        int slotIndex = 0;
        for (const std::string& rId : {rChassis, std::string("test_terraformer")})
        {
            const UnitComponentConfig_t* pComponent =
                (pTerraformer && rId == "test_terraformer")
                    ? pTerraformer
                    : fixtures.unitComponents.Find(rId);
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
    CHECK(std::get<TerraformOrder_t>(*former.GetOrder()).projectId == "Road");
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
    CHECK(rocky.GetRockiness() == Rockiness_t::Rocky);
    CHECK_FALSE(rocky.HasImprovement("Farm"));
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
    CHECK(tile.HasFeature("Fungus"));

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "RemoveFungus", *game.pState));
    game.FinishTerraform(former);
    CHECK_FALSE(tile.HasFeature("Fungus"));

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "Aquifer", *game.pState));
    game.FinishTerraform(former);
    CHECK(tile.GetHasAquifer());
    CHECK(tile.GetHasRiver());
}

TEST_CASE("A terrain operation the code has never heard of runs end to end",
          "[unit][terraform][mutate]")
{
    // The point of composing operations out of triggered effects: "Rockify" matches no C++
    // enumerator and no shipping config entry, and still starts, spends turns, and lands.
    TerraformGame_ game;
    TerrainOperationRegistry& rOperations = *game.fixtures.dataContext.terrainOperationRegistry;
    std::vector<TerrainOperationConfig_t> operations = rOperations.GetAll();
    TerrainOperationConfig_t rockify;
    rockify.id = "Rockify";
    rockify.name = "Rockify";
    rockify.project.turnsRequired = 1;
    TriggeredEffectConfig_t roughen;
    roughen.effect = StepRockinessEffect_t{1};
    rockify.onCompleteEffects.push_back(roughen);
    operations.push_back(std::move(rockify));
    rOperations.Assign(std::move(operations));

    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home);
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);
    REQUIRE(tile.GetRockiness() == Rockiness_t::Flat);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "Rockify",
                                                                 *game.pState));
    game.FinishTerraform(former);
    CHECK(tile.GetRockiness() == Rockiness_t::Rolling);
}

TEST_CASE("A project whose effects would all no-op is refused before it is paid for",
          "[unit][terraform][mutate]")
{
    // Each effect's own condition is what says whether the project may start, so a Former is
    // never charged for an order that would complete having changed nothing.
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home);
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);
    REQUIRE(tile.GetRockiness() == Rockiness_t::Flat);

    // Nothing to level on flat ground, and no fungus to remove.
    const int energyBefore = game.pPlayer->GetEconomy().GetEnergy();
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "LevelTerrain",
                                                                     *game.pState));
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "RemoveFungus",
                                                                     *game.pState));
    CHECK(game.pPlayer->GetEconomy().GetEnergy() == energyBefore);
    CHECK_FALSE(former.GetOrder().has_value());

    // And the same project is allowed the moment its condition holds.
    tile.SetRockiness(Rockiness_t::Rocky);
    CHECK(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "LevelTerrain",
                                                               *game.pState));
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
    const ElevationRulesConfig_t& rRules = game.fixtures.dataContext.elevationRules;
    const int raised = tile.GetElevation();
    CHECK(raised >= 1000 + rRules.levelMinMeters);
    CHECK(raised <= 1000 + rRules.levelMaxMeters);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(
        former, "LowerLand", *game.pState));
    game.FinishTerraform(former);
    const int lowered = tile.GetElevation();
    CHECK(lowered >= raised - rRules.levelMaxMeters);
    CHECK(lowered <= raised - rRules.levelMinMeters);
}

TEST_CASE("Lowering land stops at Planet's floor instead of throwing",
          "[unit][terraform][mutate]")
{
    // A sea Former's LowerLand gate was unconditional, so on a deep-ocean tile the mutation
    // drove elevation past the map's lower bound. Tile::SetElevation now rejects that, which
    // would throw out of order execution - nothing catches between there and main().
    // A fixed 1000 m level: 500 m of headroom takes the roll down to the floor rather than
    // refusing it. The order already charged energy and spent its turns, and whether the roll
    // overshoots is a die the player never saw.
    ElevationRulesConfig_t rules = actest::TestMapRules();
    rules.levelMinMeters = 1000;
    rules.levelMaxMeters = 1000;
    TerraformGame_ game(/*bExcludes=*/false, &rules);
    BaseManager& home = game.MakeBase(4, 4);
    Unit& seaFormer = game.MakeFormer(6, 4, &home, nullptr, "test_sea_chassis");
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);
    const int floor = rules.minElevationMeters;
    tile.SetElevation(floor + 500);

    std::mt19937 rng(1);
    const std::optional<TerraformProject_t> lowerLand =
        FindTerraformProject("LowerLand", game.pState->GetTileEffects().GetImprovements(),
                             *game.fixtures.dataContext.terrainOperationRegistry);
    REQUIRE(lowerLand);
    CHECK(ApplyTerraformResult(tile, *lowerLand, game.pState->GetTileEffects(), seaFormer, rng));
    CHECK(tile.GetElevation() == floor);

    tile.SetElevation(floor + 1000);
    CHECK(ApplyTerraformResult(tile, *lowerLand, game.pState->GetTileEffects(), seaFormer, rng));
    CHECK(tile.GetElevation() == floor);

    // Already on the floor: nothing to lower, and no exception.
    CHECK_FALSE(
        ApplyTerraformResult(tile, *lowerLand, game.pState->GetTileEffects(), seaFormer, rng));
    CHECK(tile.GetElevation() == floor);
}

TEST_CASE("A land Former's lower stops at ocean level instead of failing the order",
          "[unit][terraform][mutate]")
{
    // A fixed 1500 m level on a 1000 m tile: the roll overshoots ocean level, which used to
    // return false after CanStartTerraform had already charged for it.
    ElevationRulesConfig_t rules = actest::TestMapRules();
    rules.levelMinMeters = 1500;
    rules.levelMaxMeters = 1500;
    TerraformGame_ game(/*bExcludes=*/false, &rules);
    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home);
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);
    tile.SetElevation(rules.referenceLevelMeters);

    std::mt19937 rng(1);
    const std::optional<TerraformProject_t> lowerLand =
        FindTerraformProject("LowerLand", game.pState->GetTileEffects().GetImprovements(),
                             *game.fixtures.dataContext.terrainOperationRegistry);
    REQUIRE(lowerLand);
    CHECK(ApplyTerraformResult(tile, *lowerLand, game.pState->GetTileEffects(), former, rng));
    CHECK(tile.GetElevation() == rules.oceanLevelMeters);
    CHECK(tile.IsLand());
}

TEST_CASE("ApplyTerraformResult places Farm via rules helper", "[unit][terraform]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home);
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);

    const std::optional<TerraformProject_t> farm =
        FindTerraformProject("Farm", game.pState->GetTileEffects().GetImprovements(),
                             *game.fixtures.dataContext.terrainOperationRegistry);
    REQUIRE(farm);
    std::mt19937 rng(1);
    REQUIRE(ApplyTerraformResult(tile, *farm, game.pState->GetTileEffects(), former, rng));
    CHECK(tile.HasImprovement("Farm"));
}

TEST_CASE("Terraform replaces improvements that cannot share the tile", "[unit][terraform]")
{
    TerraformGame_ game(/*bExcludes=*/true);
    BaseManager& home = game.MakeBase(4, 4);
    const ImprovementRegistry& rImprovements = game.fixtures.improvements;

    Tile& rDirect = *game.pState->GetWorldMap().GetTile(5, 4);
    game.pState->GetTileEffects().AddOccupantWithEffects(rDirect, "Forest");
    game.pState->GetTileEffects().AddOccupantWithEffects(rDirect, "Road");
    game.pState->GetTileEffects().AddOccupantWithEffects(rDirect, "Mine");
    CHECK_FALSE(rDirect.HasImprovement("Forest"));
    CHECK(rDirect.HasImprovement("Road"));
    CHECK(rDirect.HasImprovement("Mine"));

    Tile& rFarmTile = *game.pState->GetWorldMap().GetTile(6, 4);
    game.pState->GetTileEffects().AddOccupantWithEffects(rFarmTile, "Forest");
    game.pState->GetTileEffects().AddOccupantWithEffects(rFarmTile, "Road");
    game.pState->GetTileEffects().AddOccupantWithEffects(rFarmTile, "Nutrients");

    const std::optional<TerraformProject_t> farm = FindTerraformProject(
        "Farm", rImprovements, *game.fixtures.dataContext.terrainOperationRegistry);
    REQUIRE(farm);
    REQUIRE(farm->pPlaces);
    const std::vector<std::string> farmRemoves =
        ImprovementsDisplacedBy(rFarmTile, *farm->pPlaces);
    CHECK(std::find(farmRemoves.begin(), farmRemoves.end(), "Forest") != farmRemoves.end());
    CHECK(std::find(farmRemoves.begin(), farmRemoves.end(), "Road") == farmRemoves.end());
    CHECK(std::find(farmRemoves.begin(), farmRemoves.end(), "Nutrients") == farmRemoves.end());

    Unit& former = game.MakeFormer(6, 4, &home);
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "Farm", *game.pState));
    game.FinishTerraform(former);
    CHECK(rFarmTile.HasImprovement("Farm"));
    CHECK_FALSE(rFarmTile.HasImprovement("Forest"));
    CHECK(rFarmTile.HasImprovement("Road"));
    CHECK(rFarmTile.HasFeature("Nutrients"));

    Unit& rockyFormer = game.MakeFormer(7, 4, &home);
    Tile& rRocky = *game.pState->GetWorldMap().GetTile(7, 4);
    rRocky.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStartTerraform(rockyFormer, "Farm",
                                                                      *game.pState));
    CHECK(rRocky.GetRockiness() == Rockiness_t::Rocky);
    CHECK_FALSE(rRocky.HasImprovement("Farm"));

    Unit& fungusFormer = game.MakeFormer(8, 4, &home);
    Tile& rBlockedFungus = *game.pState->GetWorldMap().GetTile(8, 4);
    game.pState->GetTileEffects().AddOccupantWithEffects(rBlockedFungus, "Fungus");
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStartTerraform(fungusFormer, "Farm",
                                                                      *game.pState));
    CHECK_FALSE(rBlockedFungus.HasImprovement("Farm"));
    CHECK(rBlockedFungus.HasFeature("Fungus"));

    Tile& rFungusTile = *game.pState->GetWorldMap().GetTile(3, 4);
    game.pState->GetTileEffects().AddOccupantWithEffects(rFungusTile, "Farm");
    game.pState->GetTileEffects().AddOccupantWithEffects(rFungusTile, "Road");
    game.pState->GetTileEffects().AddOccupantWithEffects(rFungusTile, "Nutrients");
    const std::optional<TerraformProject_t> plant = FindTerraformProject(
        "PlantFungus", rImprovements, *game.fixtures.dataContext.terrainOperationRegistry);
    REQUIRE(plant);

    std::mt19937 rng(1);
    REQUIRE(
        ApplyTerraformResult(rFungusTile, *plant, game.pState->GetTileEffects(), former, rng));
    CHECK(rFungusTile.HasFeature("Fungus"));
    CHECK_FALSE(rFungusTile.HasImprovement("Farm"));
    CHECK_FALSE(rFungusTile.HasImprovement("Road"));
    CHECK(rFungusTile.HasFeature("Nutrients"));
}

TEST_CASE("A coexistence override lets Farm share a rocky tile", "[unit][terraform]")
{
    UnitComponentConfig_t waiver;
    TerraformGame_ game;
    const UnitComponentConfig_t* pStock = game.fixtures.unitComponents.Find("test_terraformer");
    REQUIRE(pStock);
    waiver = *pStock;
    EffectConfig_t effect;
    effect.scope = EffectScope_t::ThisUnit;
    CoexistenceOverrideEffect_t overrideFx;
    overrideFx.cell = InteractionCell_t::Allow;
    overrideFx.firstId = "Farm";
    overrideFx.secondId = "Rocky";
    effect.effect = overrideFx;
    waiver.effects.push_back(effect);

    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home, &waiver);
    Tile& rocky = *game.pState->GetWorldMap().GetTile(6, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "Farm", *game.pState));
    game.FinishTerraform(former);
    CHECK(rocky.HasImprovement("Farm"));
    CHECK(rocky.GetRockiness() == Rockiness_t::Rocky);
}

TEST_CASE("A coexistence waiver outlives the former that earned it", "[unit][terraform]")
{
    TerraformGame_ game;
    const UnitComponentConfig_t* pStock = game.fixtures.unitComponents.Find("test_terraformer");
    REQUIRE(pStock);
    UnitComponentConfig_t waiver = *pStock;
    EffectConfig_t effect;
    effect.scope = EffectScope_t::ThisUnit;
    CoexistenceOverrideEffect_t overrideFx;
    overrideFx.cell = InteractionCell_t::Allow;
    overrideFx.firstId = "Farm";
    overrideFx.secondId = "Rocky";
    effect.effect = overrideFx;
    waiver.effects.push_back(effect);

    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home, &waiver);
    Tile& rocky = *game.pState->GetWorldMap().GetTile(6, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "Farm", *game.pState));
    game.FinishTerraform(former);
    REQUIRE(rocky.HasImprovement("Farm"));

    // The occupancy sweep any later change runs carries no overrides, so without the waiver
    // recorded on the tile the Farm would be evicted by the Rocky it was licensed to share.
    game.pState->GetTileEffects().AddOccupantWithEffects(rocky, "Road");
    CHECK(rocky.HasImprovement("Farm"));
    CHECK(rocky.HasImprovement("Road"));
}

TEST_CASE("A coexistence waiver dies with the occupant it was granted for",
          "[unit][terraform]")
{
    // The licence is for one pair. Once the occupant is gone the waiver is spent: keeping it
    // would silently re-apply if that occupant ever came back, to an improvement no former
    // re-earned it for.
    TerraformGame_ game;
    const UnitComponentConfig_t* pStock = game.fixtures.unitComponents.Find("test_terraformer");
    REQUIRE(pStock);
    UnitComponentConfig_t waiver = *pStock;
    EffectConfig_t effect;
    effect.scope = EffectScope_t::ThisUnit;
    CoexistenceOverrideEffect_t overrideFx;
    overrideFx.cell = InteractionCell_t::Allow;
    overrideFx.firstId = "Farm";
    overrideFx.secondId = "Rocky";
    effect.effect = overrideFx;
    waiver.effects.push_back(effect);

    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home, &waiver);
    Tile& rocky = *game.pState->GetWorldMap().GetTile(6, 4);
    rocky.SetRockiness(Rockiness_t::Rocky);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryStartTerraform(former, "Farm", *game.pState));
    game.FinishTerraform(former);
    REQUIRE(rocky.HasImprovement("Farm"));
    REQUIRE_FALSE(rocky.GetCoexistenceWaivers().empty());

    // Level the tile: the Rocky the waiver named is gone, so the waiver goes with it.
    rocky.SetRockiness(Rockiness_t::Flat);
    CHECK(rocky.GetCoexistenceWaivers().empty());
    CHECK(rocky.HasImprovement("Farm"));

    // Rocky returning finds no standing licence, so the Farm it excludes is evicted.
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(rocky.HasImprovement("Farm"));
}

TEST_CASE("An improvement with no waiver still loses to terrain it cannot share",
          "[unit][terraform]")
{
    TerraformGame_ game;
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);
    game.pState->GetTileEffects().AddOccupantWithEffects(tile, "Farm");
    REQUIRE(tile.HasImprovement("Farm"));

    tile.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(tile.HasImprovement("Farm"));
}

TEST_CASE("A terraform result that cannot place destroys nothing", "[unit][terraform]")
{
    // The order has already been paid for. If terrain shifted while it ran, the tile keeps
    // what it had rather than losing the incumbent to a placement that then refuses.
    TerraformGame_ game(/*bExcludes=*/true);
    BaseManager& home = game.MakeBase(4, 4);
    Unit& former = game.MakeFormer(6, 4, &home);
    Tile& tile = *game.pState->GetWorldMap().GetTile(6, 4);

    // Road is the incumbent because it shares a tile with Rocky quite happily; Farm is the
    // project because it excludes Rocky, so the placement refuses on terrain it cannot clear.
    game.pState->GetTileEffects().AddOccupantWithEffects(tile, "Road");
    REQUIRE(tile.HasImprovement("Road"));
    tile.SetRockiness(Rockiness_t::Rocky);
    REQUIRE(tile.HasImprovement("Road"));

    const std::optional<TerraformProject_t> farm = FindTerraformProject(
        "Farm", game.fixtures.improvements,
        *game.fixtures.dataContext.terrainOperationRegistry);
    REQUIRE(farm);
    std::mt19937 rng(3);
    CHECK_FALSE(
        ApplyTerraformResult(tile, *farm, game.pState->GetTileEffects(), former, rng));
    CHECK(tile.HasImprovement("Road"));
    CHECK_FALSE(tile.HasImprovement("Farm"));
}

