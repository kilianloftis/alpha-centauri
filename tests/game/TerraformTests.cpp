#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/economy/EconomyManager.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/TerraformProjectRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/map/MapRulesConfig.h"
#include "game/research/ResearchManager.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
#include "game/terraform/ElevationRules.h"
#include "game/terraform/FormerEnergyCostCalculator.h"
#include "game/terraform/TerraformRules.h"
#include "game/terraform/TerraformMutations.h"
#include "game/units/Unit.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "TestHelpers.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/UnitSlotConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "lib/config/ConfigReader.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
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
    Faction* pPlayer = nullptr;

    explicit TerraformGame_(bool bExcludes = false, const MapRulesConfig_t* pRules = nullptr)
        : fixtures(actest::k_TestMapWidth, actest::k_TestMapHeight, {},
                   bExcludes ? k_ExcludesOccupantFiles : OccupantFiles_t{},
                   pRules ? *pRules : actest::TestMapRules())
    {
        FillLand(fixtures.map);
        for (auto& pTile : fixtures.map.GetTiles())
        {
            pTile->SetRockiness(Rockiness_t::Flat);
        }

        auto pFaction = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.map, fixtures.settings, fixtures.state.GetUnitIdAllocator(), actest::k_TestFactionSeed);
        pPlayer = &fixtures.state.AddFaction(std::move(pFaction));
        pPlayer->GetEconomy().AddEnergy(100);
    }

    BaseManager& MakeBase(int x, int y)
    {
        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = pPlayer->CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", pTile,
            fixtures.state.GetTileEffects(), std::nullopt);
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
        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return pPlayer->GetUnitManager().CreateUnit(
            fixtures.state.AllocateUnitId(), rDesign, fixtures.state.GetWorldMap().GetUnitPositions(), *pTile, ac::UnitOrigin::Gift(pHome));
    }

    // The caller's path: CanStartTerraformProject, then the mutation. False when it may not.
    bool StartTerraform(Unit& rFormer, const std::string& projectId)
    {
        if (!CanStartTerraformProject(rFormer, projectId))
        {
            return false;
        }
        ac::StartTerraform(fixtures.state, rFormer, projectId);
        return true;
    }

    void FinishTerraform(Unit& rUnit)
    {
        UnitOrderExecutor& rExec = fixtures.state.GetUnitOrderExecutor();
        while (rUnit.GetOrder().has_value()
               && std::holds_alternative<TerraformOrder_t>(*rUnit.GetOrder()))
        {
            REQUIRE(rExec.Execute(rUnit) != OrderProgress_t::Expended);
        }
    }
};

} // namespace

TEST_CASE("StartTerraform places Road after turns complete", "[unit][terraform]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    REQUIRE(ResolveFlag(former, RuleFlagId_t::Terraform));

    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    REQUIRE_FALSE(tile.HasImprovement("Road"));

    REQUIRE(game.StartTerraform(former, "Road"));
    REQUIRE(former.GetOrder().has_value());
    CHECK(std::get<TerraformOrder_t>(*former.GetOrder()).projectId == "Road");
    CHECK(former.GetMoveFragmentsRemaining() == 0);

    game.FinishTerraform(former);
    CHECK(tile.HasImprovement("Road"));
    CHECK_FALSE(former.GetOrder().has_value());
}

TEST_CASE("StartTerraform rejects non-formers and exclusions", "[unit][terraform]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);

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
    Tile* pTile = game.fixtures.state.GetWorldMap().GetTile(10, 10);
    Unit& scout = game.pPlayer->GetUnitManager().CreateUnit(
        game.fixtures.state.AllocateUnitId(), game.fixtures.designs.back(),
        game.fixtures.state.GetWorldMap().GetUnitPositions(), *pTile, ac::UnitOrigin::Issued(home));

    CHECK_FALSE(game.StartTerraform(scout, "Farm"));

    Unit& former = game.MakeFormer(11, 11, &home);
    Tile& rocky = *game.fixtures.state.GetWorldMap().GetTile(11, 11);
    rocky.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(game.StartTerraform(former, "Farm"));
    CHECK(rocky.GetRockiness() == Rockiness_t::Rocky);
    CHECK_FALSE(rocky.HasImprovement("Farm"));
}

TEST_CASE("Terraform mutations: level, fungus, aquifer", "[unit][terraform][mutate]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    tile.SetRockiness(Rockiness_t::Rocky);

    REQUIRE(game.StartTerraform(former, "LevelTerrain"));
    game.FinishTerraform(former);
    CHECK(tile.GetRockiness() == Rockiness_t::Rolling);

    REQUIRE(game.StartTerraform(former, "PlantFungus"));
    game.FinishTerraform(former);
    CHECK(tile.HasFeature("Fungus"));

    REQUIRE(game.StartTerraform(former, "RemoveFungus"));
    game.FinishTerraform(former);
    CHECK_FALSE(tile.HasFeature("Fungus"));

    REQUIRE(game.StartTerraform(former, "Aquifer"));
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
    TerraformProjectRegistry& rOperations = *game.fixtures.dataContext.terraformProjectRegistry;
    std::vector<TerraformProjectConfig_t> operations = rOperations.GetAll();
    TerraformProjectConfig_t rockify;
    rockify.id = "Rockify";
    rockify.name = "Rockify";
    rockify.project.turnsRequired = 1;
    TriggeredEffectConfig_t roughen;
    roughen.effect = StepRockinessEffect_t{1};
    rockify.onCompleteEffects.push_back(roughen);
    operations.push_back(std::move(rockify));
    rOperations.Assign(std::move(operations));

    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    REQUIRE(tile.GetRockiness() == Rockiness_t::Flat);

    REQUIRE(game.StartTerraform(former, "Rockify"));
    game.FinishTerraform(former);
    CHECK(tile.GetRockiness() == Rockiness_t::Rolling);
}

TEST_CASE("A project whose effects would all no-op is refused before it is paid for",
          "[unit][terraform][mutate]")
{
    // Each effect's own condition is what says whether the project may start, so a Former is
    // never charged for an order that would complete having changed nothing.
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    REQUIRE(tile.GetRockiness() == Rockiness_t::Flat);

    // Nothing to level on flat ground, and no fungus to remove.
    const int energyBefore = game.pPlayer->GetEconomy().GetEnergy();
    CHECK_FALSE(game.StartTerraform(former, "LevelTerrain"));
    CHECK_FALSE(game.StartTerraform(former, "RemoveFungus"));
    CHECK(game.pPlayer->GetEconomy().GetEnergy() == energyBefore);
    CHECK_FALSE(former.GetOrder().has_value());

    // And the same project is allowed the moment its condition holds.
    tile.SetRockiness(Rockiness_t::Rocky);
    CHECK(game.StartTerraform(former, "LevelTerrain"));
}

TEST_CASE("A sea Former cannot raise across the waterline", "[unit][terraform][elevation]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Tile& sea = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    const MapRulesConfig_t& rRules = sea.MapRules();
    sea.SetElevation(rRules.oceanLevelMeters - 1);
    REQUIRE(sea.IsWater());
    Unit& seaFormer = game.MakeFormer(10, 10, &home, nullptr, "test_sea_chassis");

    CHECK_FALSE(FormerRaiseAllowed(seaFormer, rRules));
    CHECK_FALSE(game.StartTerraform(seaFormer, "RaiseLand"));

    sea.SetElevation(rRules.oceanLevelMeters - rRules.levelMaxMeters - 1);
    CHECK(FormerRaiseAllowed(seaFormer, rRules));
}

TEST_CASE("A land Former cannot raise from the top altitude band", "[unit][terraform][elevation]")
{
    // SMAC TOOHIGH: refuse raise from altitude 6 — elev >= max − reference_level.
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    const MapRulesConfig_t& rRules = tile.MapRules();
    const int topBandStart = rRules.maxElevationMeters - rRules.referenceLevelMeters;

    tile.SetElevation(topBandStart);
    CHECK_FALSE(FormerRaiseAllowed(former, rRules));
    CHECK_FALSE(game.StartTerraform(former, "RaiseLand"));

    tile.SetElevation(topBandStart - 1);
    CHECK(FormerRaiseAllowed(former, rRules));
}

TEST_CASE("Land-domain projects refuse a sea tile", "[unit][terraform][domain]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Tile& sea = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    sea.SetElevation(-100);
    REQUIRE(sea.IsWater());
    Unit& seaFormer = game.MakeFormer(10, 10, &home, nullptr, "test_sea_chassis");
    sea.SetRockiness(Rockiness_t::Rocky);

    CHECK_FALSE(game.StartTerraform(seaFormer, "Road"));
    CHECK_FALSE(game.StartTerraform(seaFormer, "Sensor"));
    CHECK_FALSE(game.StartTerraform(seaFormer, "LevelTerrain"));
    CHECK_FALSE(game.StartTerraform(seaFormer, "Aquifer"));
}

TEST_CASE("Deep ocean refuses every project but raise and lower", "[unit][terraform][lowtidal]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Tile& deep = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    const MapRulesConfig_t& rRules = deep.MapRules();
    deep.SetElevation(rRules.oceanShelfMeters - 1);
    REQUIRE(deep.IsWater());
    REQUIRE(deep.HasFeature("Ocean"));
    Unit& seaFormer = game.MakeFormer(10, 10, &home, nullptr, "test_sea_chassis");

    CHECK_FALSE(game.StartTerraform(seaFormer, "PlantFungus"));
    CHECK_FALSE(game.StartTerraform(seaFormer, "KelpFarm"));
    CHECK(game.StartTerraform(seaFormer, "RaiseLand"));
}

TEST_CASE("Monolith allows Road and PlantFungus but not Farm", "[unit][terraform][monolith]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(tile, "Monolith");
    Unit& former = game.MakeFormer(10, 10, &home);

    CHECK_FALSE(game.StartTerraform(former, "Farm"));
    REQUIRE(game.StartTerraform(former, "Road"));
    game.FinishTerraform(former);
    CHECK(tile.HasImprovement("Road"));
    CHECK(tile.HasFeature("Monolith"));

    REQUIRE(game.StartTerraform(former, "PlantFungus"));
    game.FinishTerraform(former);
    CHECK(tile.HasFeature("Fungus"));
}

TEST_CASE("Raise/lower energy quote applies enemy proximity scale", "[unit][terraform][energy]")
{
    // docs/thinker/terranx-map.md `terraform_cost`: enemy_val/own_val when enemy_val > own_val.
    TerraformGame_ game;
    FormerEnergyCostCalculator& rCalc = *game.fixtures.dataContext.formerEnergyCostCalculator;
    const nlohmann::json j = {{"energy_cost", "terraform_cost.lua"}};
    const ConfigReader reader(j, "test", FixturePath("terrain.json"));
    FormerEnergyCost_t cost;
    cost.formula = reader.Formula(
        "energy_cost", {"elev_band", "fungus", "at_sea", "sea_cost_scale",
                        "nearest_base_distance", "own_pop", "enemy_pop", "own_range",
                        "enemy_range"});

    FormerEnergyCostInputs_t inputs;
    inputs.elevBand = 1;
    inputs.nearestBaseDistance = 2;
    // Band 1, land, no enemy → floor(9 * 2 / 2) = 9.
    CHECK(rCalc.Cost(cost, inputs) == 9);

    inputs.ownPop = 1;
    inputs.enemyPop = 10;
    inputs.ownRange = 6;
    inputs.enemyRange = 3;
    // enemy_val/own_val = 8 → floor(9 * 2 * 8 / 2) = 72.
    CHECK(rCalc.Cost(cost, inputs) == 72);
}

TEST_CASE("Raise/lower energy quote matches the SMAC terraform_cost shape",
          "[unit][terraform][energy]")
{
    // docs/thinker/terranx-map.md `terraform_cost`: (d+2)^2 * fungus/sea * range / 2.
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    tile.SetElevation(1000); // elev_band 1 with reference_level 1000

    const std::optional<TerraformProject_t> raise =
        FindTerraformProject("RaiseLand", game.fixtures.state.GetTileEffects().GetImprovements(),
                             *game.fixtures.dataContext.terraformProjectRegistry);
    REQUIRE(raise);
    // Band 1, no fungus, land, range from (10,10) to (8,8) = 2 → floor(9 * 1 * 1 * 2 / 2) = 9.
    CHECK(TerraformEnergyCost(former, *raise) == 9);

    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(tile, "Fungus");
    // Fungus ×6 → floor(9 * 6 * 2 / 2) = 54.
    CHECK(TerraformEnergyCost(former, *raise) == 54);
}

TEST_CASE("Sea raise energy uses SeaRaiseLowerCostScale from Doctrine: Air Power",
          "[unit][terraform][energy]")
{
    // docs/thinker/terranx-map.md `terraform_cost`: ×4 at sea, ×2 once any faction has Doc Air.
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Tile& sea = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    sea.SetElevation(-1000); // elev_band 1, below ocean_level 0
    REQUIRE(sea.IsWater());
    Unit& seaFormer = game.MakeFormer(10, 10, &home, nullptr, "test_sea_chassis");

    const std::optional<TerraformProject_t> raise =
        FindTerraformProject("RaiseLand", game.fixtures.state.GetTileEffects().GetImprovements(),
                             *game.fixtures.dataContext.terraformProjectRegistry);
    REQUIRE(raise);
    // Band 1, sea ×4, range 2 → floor(9 * 4 * 2 / 2) = 36.
    CHECK(TerraformEnergyCost(seaFormer, *raise) == 36);

    Faction& peer = game.fixtures.MakeFaction();
    peer.GetResearch().AddDiscoveredTech("doctrine_air_power");
    // Peer WorldGlobal MaxClamp 0.5 → sea_cost_seed*scale = 2 → floor(9 * 2 * 2 / 2) = 18.
    CHECK(TerraformEnergyCost(seaFormer, *raise) == 18);
}

TEST_CASE("Raise and lower land change elevation", "[unit][terraform][mutate]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    tile.SetElevation(1000);

    const int energyBefore = game.pPlayer->GetEconomy().GetEnergy();
    REQUIRE(game.StartTerraform(former, "RaiseLand"));
    CHECK(game.pPlayer->GetEconomy().GetEnergy() < energyBefore);
    game.FinishTerraform(former);
    const MapRulesConfig_t& rRules = tile.MapRules();
    const int raised = tile.GetElevation();
    CHECK(raised >= 1000 + rRules.levelMinMeters);
    CHECK(raised <= 1000 + rRules.levelMaxMeters);

    REQUIRE(game.StartTerraform(former, "LowerLand"));
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
    MapRulesConfig_t rules = actest::TestMapRules();
    rules.levelMinMeters = 1000;
    rules.levelMaxMeters = 1000;
    TerraformGame_ game(/*bExcludes=*/false, &rules);
    BaseManager& home = game.MakeBase(8, 8);
    Unit& seaFormer = game.MakeFormer(10, 10, &home, nullptr, "test_sea_chassis");
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    const int floor = rules.minElevationMeters;
    tile.SetElevation(floor + 500);

    std::mt19937 rng(1);
    const std::optional<TerraformProject_t> lowerLand =
        FindTerraformProject("LowerLand", game.fixtures.state.GetTileEffects().GetImprovements(),
                             *game.fixtures.dataContext.terraformProjectRegistry);
    REQUIRE(lowerLand);
    CHECK(ApplyTerraformResult(tile, *lowerLand, game.fixtures.state.GetTileEffects(), seaFormer, rng,
                               game.fixtures.state));
    CHECK(tile.GetElevation() == floor);

    tile.SetElevation(floor + 1000);
    CHECK(ApplyTerraformResult(tile, *lowerLand, game.fixtures.state.GetTileEffects(), seaFormer, rng,
                               game.fixtures.state));
    CHECK(tile.GetElevation() == floor);

    // Already on the floor: nothing to lower, and no exception.
    CHECK_FALSE(
        ApplyTerraformResult(tile, *lowerLand, game.fixtures.state.GetTileEffects(), seaFormer, rng,
                               game.fixtures.state));
    CHECK(tile.GetElevation() == floor);
}

TEST_CASE("A paid terraform that finishes with no effect keeps energy and notifies the player",
          "[unit][terraform][notice]")
{
    // Start while the roll can move the tile, then put it on the floor before completion —
    // mid-order surface change is the paid no-effect path (docs/thinker/terranx-map.md "Work").
    MapRulesConfig_t rules = actest::TestMapRules();
    rules.levelMinMeters = 1000;
    rules.levelMaxMeters = 1000;
    TerraformGame_ game(/*bExcludes=*/false, &rules);
    BaseManager& home = game.MakeBase(8, 8);
    Unit& seaFormer = game.MakeFormer(10, 10, &home, nullptr, "test_sea_chassis");
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    const int floor = rules.minElevationMeters;
    tile.SetElevation(floor + 1000);

    REQUIRE(game.StartTerraform(seaFormer, "LowerLand"));
    const int energyAfterPay = game.pPlayer->GetEconomy().GetEnergy();
    tile.SetElevation(floor);

    game.FinishTerraform(seaFormer);
    CHECK(game.pPlayer->GetEconomy().GetEnergy() == energyAfterPay);
    CHECK(tile.GetElevation() == floor);

    PlayerInteractionQueue& rQueue = game.fixtures.state.GetPlayerInteractions();
    REQUIRE(rQueue.Size() == 1);
    const auto* pNotice =
        std::get_if<TerraformNoEffectInteraction_t>(&rQueue.Front()->payload);
    REQUIRE(pNotice);
    CHECK(pNotice->projectId == "LowerLand");
    CHECK(pNotice->cameraTile == tile.GetCoord());
}

TEST_CASE("A land Former cannot lower through the waterline", "[unit][terraform][elevation]")
{
    // Shore (ocean level): one SMAC altitude step down is water under a land Former.
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    const MapRulesConfig_t& rRules = tile.MapRules();
    tile.SetElevation(rRules.oceanLevelMeters);
    REQUIRE(tile.IsLand());

    CHECK_FALSE(FormerLowerAllowed(former, rRules));
    CHECK_FALSE(game.StartTerraform(former, "LowerLand"));
}

TEST_CASE("ApplyTerraformResult places Farm via rules helper", "[unit][terraform]")
{
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);

    const std::optional<TerraformProject_t> farm =
        FindTerraformProject("Farm", game.fixtures.state.GetTileEffects().GetImprovements(),
                             *game.fixtures.dataContext.terraformProjectRegistry);
    REQUIRE(farm);
    std::mt19937 rng(1);
    REQUIRE(ApplyTerraformResult(tile, *farm, game.fixtures.state.GetTileEffects(), former, rng,
                               game.fixtures.state));
    CHECK(tile.HasImprovement("Farm"));
}

TEST_CASE("Terraform replaces improvements that cannot share the tile", "[unit][terraform]")
{
    TerraformGame_ game(/*bExcludes=*/true);
    BaseManager& home = game.MakeBase(8, 8);
    const ImprovementRegistry& rImprovements = game.fixtures.improvements;

    Tile& rDirect = *game.fixtures.state.GetWorldMap().GetTile(9, 9);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rDirect, "Forest");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rDirect, "Road");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rDirect, "Mine");
    CHECK_FALSE(rDirect.HasImprovement("Forest"));
    CHECK(rDirect.HasImprovement("Road"));
    CHECK(rDirect.HasImprovement("Mine"));

    Tile& rFarmTile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rFarmTile, "Forest");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rFarmTile, "Road");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rFarmTile, "Nutrients");

    const std::optional<TerraformProject_t> farm = FindTerraformProject(
        "Farm", rImprovements, *game.fixtures.dataContext.terraformProjectRegistry);
    REQUIRE(farm);
    REQUIRE(farm->pPlaces);
    const std::vector<std::string> farmRemoves =
        ImprovementsDisplacedBy(rFarmTile, *farm->pPlaces, {});
    CHECK(std::find(farmRemoves.begin(), farmRemoves.end(), "Forest") != farmRemoves.end());
    CHECK(std::find(farmRemoves.begin(), farmRemoves.end(), "Road") == farmRemoves.end());
    CHECK(std::find(farmRemoves.begin(), farmRemoves.end(), "Nutrients") == farmRemoves.end());

    Unit& former = game.MakeFormer(10, 10, &home);
    REQUIRE(game.StartTerraform(former, "Farm"));
    game.FinishTerraform(former);
    CHECK(rFarmTile.HasImprovement("Farm"));
    CHECK_FALSE(rFarmTile.HasImprovement("Forest"));
    CHECK(rFarmTile.HasImprovement("Road"));
    CHECK(rFarmTile.HasFeature("Nutrients"));

    Unit& rockyFormer = game.MakeFormer(11, 11, &home);
    Tile& rRocky = *game.fixtures.state.GetWorldMap().GetTile(11, 11);
    rRocky.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(game.StartTerraform(rockyFormer, "Farm"));
    CHECK(rRocky.GetRockiness() == Rockiness_t::Rocky);
    CHECK_FALSE(rRocky.HasImprovement("Farm"));

    Unit& fungusFormer = game.MakeFormer(12, 12, &home);
    Tile& rBlockedFungus = *game.fixtures.state.GetWorldMap().GetTile(12, 12);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rBlockedFungus, "Fungus");
    CHECK_FALSE(game.StartTerraform(fungusFormer, "Farm"));
    CHECK_FALSE(rBlockedFungus.HasImprovement("Farm"));
    CHECK(rBlockedFungus.HasFeature("Fungus"));

    Tile& rFungusTile = *game.fixtures.state.GetWorldMap().GetTile(7, 7);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rFungusTile, "Farm");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rFungusTile, "Road");
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rFungusTile, "Nutrients");
    const std::optional<TerraformProject_t> plant = FindTerraformProject(
        "PlantFungus", rImprovements, *game.fixtures.dataContext.terraformProjectRegistry);
    REQUIRE(plant);

    std::mt19937 rng(1);
    REQUIRE(
        ApplyTerraformResult(rFungusTile, *plant, game.fixtures.state.GetTileEffects(), former, rng,
                               game.fixtures.state));
    CHECK(rFungusTile.HasFeature("Fungus"));
    CHECK_FALSE(rFungusTile.HasImprovement("Farm"));
    CHECK(rFungusTile.HasImprovement("Road"));
    CHECK(rFungusTile.HasFeature("Nutrients"));
}

TEST_CASE("Fungus road and mine starts need CentEmp and CentPsi RuleFlags", "[unit][terraform]")
{
    TerraformGame_ game(true);
    BaseManager& home = game.MakeBase(8, 8);
    const UnitComponentConfig_t* pStock = game.fixtures.unitComponents.Find("test_terraformer");
    REQUIRE(pStock);
    EffectPool pool;

    UnitComponentConfig_t roadFormer = *pStock;
    roadFormer.effects.push_back(
        pool.RuleFlag(RuleFlagId_t::TerraformFungusRoad, EffectScope_t::ThisUnit));

    UnitComponentConfig_t mineFormer = *pStock;
    mineFormer.effects.push_back(
        pool.RuleFlag(RuleFlagId_t::TerraformFungusImprovement, EffectScope_t::ThisUnit));

    Tile& rRoadTile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rRoadTile, "Fungus");
    Unit& noTechRoad = game.MakeFormer(10, 10, &home);
    CHECK_FALSE(game.StartTerraform(noTechRoad, "Road"));
    Unit& empRoad = game.MakeFormer(10, 10, &home, &roadFormer);
    REQUIRE(game.StartTerraform(empRoad, "Road"));
    game.FinishTerraform(empRoad);
    CHECK(rRoadTile.HasImprovement("Road"));
    CHECK(rRoadTile.HasFeature("Fungus"));

    Tile& rMineTile = *game.fixtures.state.GetWorldMap().GetTile(11, 11);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rMineTile, "Fungus");
    Unit& noTechMine = game.MakeFormer(11, 11, &home);
    CHECK_FALSE(game.StartTerraform(noTechMine, "Mine"));
    Unit& psiMine = game.MakeFormer(11, 11, &home, &mineFormer);
    REQUIRE(game.StartTerraform(psiMine, "Mine"));
    game.FinishTerraform(psiMine);
    CHECK(rMineTile.HasImprovement("Mine"));
    CHECK(rMineTile.HasFeature("Fungus"));

    // Built-on-fungus occupancy does not need a tile waiver; later sweeps keep both.
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rMineTile, "Road");
    CHECK(rMineTile.HasImprovement("Mine"));
    CHECK(rMineTile.HasFeature("Fungus"));
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

    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home, &waiver);
    Tile& rocky = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    rocky.SetRockiness(Rockiness_t::Rocky);

    REQUIRE(game.StartTerraform(former, "Farm"));
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

    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home, &waiver);
    Tile& rocky = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    rocky.SetRockiness(Rockiness_t::Rocky);

    REQUIRE(game.StartTerraform(former, "Farm"));
    game.FinishTerraform(former);
    REQUIRE(rocky.HasImprovement("Farm"));

    // The occupancy sweep any later change runs carries no overrides, so without the waiver
    // recorded on the tile the Farm would be evicted by the Rocky it was licensed to share.
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(rocky, "Road");
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

    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home, &waiver);
    Tile& rocky = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    rocky.SetRockiness(Rockiness_t::Rocky);

    REQUIRE(game.StartTerraform(former, "Farm"));
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
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(tile, "Farm");
    REQUIRE(tile.HasImprovement("Farm"));

    tile.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(tile.HasImprovement("Farm"));
}

TEST_CASE("A terraform result that cannot place destroys nothing", "[unit][terraform]")
{
    // The order has already been paid for. If terrain shifted while it ran, the tile keeps
    // what it had rather than losing the incumbent to a placement that then refuses.
    TerraformGame_ game(/*bExcludes=*/true);
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(10, 10, &home);
    Tile& tile = *game.fixtures.state.GetWorldMap().GetTile(10, 10);

    // Road is the incumbent because it shares a tile with Rocky quite happily; Farm is the
    // project because it excludes Rocky, so the placement refuses on terrain it cannot clear.
    game.fixtures.state.GetTileEffects().AddOccupantWithEffects(tile, "Road");
    REQUIRE(tile.HasImprovement("Road"));
    tile.SetRockiness(Rockiness_t::Rocky);
    REQUIRE(tile.HasImprovement("Road"));

    const std::optional<TerraformProject_t> farm = FindTerraformProject(
        "Farm", game.fixtures.improvements,
        *game.fixtures.dataContext.terraformProjectRegistry);
    REQUIRE(farm);
    std::mt19937 rng(3);
    CHECK_FALSE(
        ApplyTerraformResult(tile, *farm, game.fixtures.state.GetTileEffects(), former, rng,
                               game.fixtures.state));
    CHECK(tile.HasImprovement("Road"));
    CHECK_FALSE(tile.HasImprovement("Farm"));
}

TEST_CASE("A former cannot build a Bunker on a base tile", "[unit][terraform][base]")
{
    TerraformGame_ game;
    game.pPlayer->GetResearch().AddDiscoveredTech("advanced_military_algorithms");
    BaseManager& home = game.MakeBase(8, 8);
    Unit& onBase = game.MakeFormer(8, 8, &home);
    Unit& offBase = game.MakeFormer(11, 11, &home);

    CHECK(game.StartTerraform(offBase, "Bunker"));
    CHECK_FALSE(game.StartTerraform(onBase, "Bunker"));
    CHECK(home.GetTile().HasImprovement("Base"));
}

TEST_CASE("A Bunker project finishing on a base tile leaves the base", "[unit][terraform][base]")
{
    // The order was paid for before a base was founded under the former.
    TerraformGame_ game;
    BaseManager& home = game.MakeBase(8, 8);
    Unit& former = game.MakeFormer(8, 8, &home);
    Tile& tile = home.GetTile();
    const std::optional<TerraformProject_t> bunker = FindTerraformProject(
        "Bunker", game.fixtures.improvements,
        *game.fixtures.dataContext.terraformProjectRegistry);
    REQUIRE(bunker);

    std::mt19937 rng(3);
    CHECK_FALSE(
        ApplyTerraformResult(tile, *bunker, game.fixtures.state.GetTileEffects(), former, rng,
                               game.fixtures.state));
    CHECK(tile.HasImprovement("Base"));
    CHECK_FALSE(tile.HasImprovement("Bunker"));
}

