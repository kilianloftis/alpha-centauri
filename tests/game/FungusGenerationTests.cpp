#include "TestTempDir.h"

#include "game/map/MapOccupantLoad.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/TerrainOperationRegistry.h"
#include "TestHelpers.h"
#include "game/worldgen/FungusGeneration.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/worldgen/WorldGenDecorationConfigParser.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <queue>
#include <random>
#include <unordered_set>
#include <vector>

using namespace ac;

namespace
{

std::filesystem::path TempPath_(const char* name)
{
    return actest::TestTempDir::Path() / name;
}

ImprovementRegistry& TestOccupants_()
{
    static ImprovementRegistry registry;
    static TerrainOperationRegistry operations;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        LoadMapOccupants(actest::FixturePath("improvements.json"),
                         actest::FixturePath("terrain.json"), registry, operations);
        bLoaded = true;
    }
    return registry;
}

const ImprovementConfig_t& TestFungus_()
{
    return TestOccupants_().Get("Fungus");
}

FungusDecorationConfig_t DefaultFungus_()
{
    FungusDecorationConfig_t cfg;
    cfg.coverage = 0.30f;
    cfg.clusterScaleTiles = 12;
    cfg.fill = 0.85f;
    cfg.poleRowChance = 0.0f;
    return cfg;
}

void FillLand_(WorldMap& rWorld)
{
    for (auto& pTile : rWorld.GetTiles())
    {
        pTile->SetElevation(1000);
    }
}

void BindAll_(WorldMap& rWorld)
{
    for (auto& pTile : rWorld.GetTiles())
    {
        pTile->BindOccupants(TestOccupants_());
    }
}

int CountFungus_(const WorldMap& rWorld)
{
    int count = 0;
    for (const auto& pTile : rWorld.GetTiles())
    {
        if (pTile->HasTerrainFeature("Fungus"))
        {
            ++count;
        }
    }
    return count;
}

int CountEligible_(WorldMap& rWorld, const ImprovementConfig_t& rFungus)
{
    int count = 0;
    for (const auto& pTile : rWorld.GetTiles())
    {
        if (!pTile->HasTerrainFeature(rFungus.id)
            && CanBuildImprovement(*pTile, rFungus, {}, {}))
        {
            ++count;
        }
    }
    return count;
}

int CountFungusComponents_(WorldMap& rWorld)
{
    std::unordered_set<const Tile*> visited;
    int components = 0;
    for (const auto& pOwned : rWorld.GetTiles())
    {
        const Tile* pStart = pOwned.get();
        if (!pStart->HasTerrainFeature("Fungus") || visited.contains(pStart))
        {
            continue;
        }
        ++components;
        std::queue<const Tile*> pending;
        pending.push(pStart);
        visited.insert(pStart);
        while (!pending.empty())
        {
            const Tile& rTile = *pending.front();
            pending.pop();
            ForEachNeighborInChebyshevRadius(rTile, rWorld, 1,
                                             [&](Tile* pNeighbor, int /*distance*/)
                                             {
                                                 if (pNeighbor->HasTerrainFeature("Fungus")
                                                     && !visited.contains(pNeighbor))
                                                 {
                                                     visited.insert(pNeighbor);
                                                     pending.push(pNeighbor);
                                                 }
                                             });
        }
    }
    return components;
}

} // namespace

TEST_CASE("WorldGenDecorationConfigParser throws when fungus object is missing",
          "[worldgen][fungus][parser]")
{
    const std::filesystem::path path = TempPath_("ac_decoration_no_fungus.json");
    {
        std::ofstream file(path);
        file << R"({
  "moisture": {
    "cloudmass_peaks": 5, "cloudmass_hills": 3, "rainfall_coeff": 1,
    "hill_min_elevation_meters": 2000, "peak_min_elevation_meters": 3000
  },
  "rockiness": {
    "poles": { "flat": 0, "rolling": 0, "rocky": 1 },
    "bands": [ { "flat": 1, "rolling": 0, "rocky": 0 } ]
  },
  "aquifers": { "fraction": 0.01 },
  "tile_bonuses": { "fraction": 0.01 }
})" << '\n';
    }

    WorldGenDecorationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("fungus"));
    std::filesystem::remove(path);
}

TEST_CASE("WorldGenDecorationConfigParser loads fungus coverage knobs",
          "[worldgen][fungus][parser]")
{
    const WorldGenDecorationConfig_t cfg =
        WorldGenDecorationConfigParser{}.ParseConfig(
            actest::FixturePath("worldGen/decoration.json"));
    CHECK(cfg.fungus.coverage == Catch::Approx(0.30f));
    CHECK(cfg.fungus.clusterScaleTiles == 12);
    CHECK(cfg.fungus.fill == Catch::Approx(0.85f));
    CHECK(cfg.fungus.poleRowChance == Catch::Approx(0.5f));
}

TEST_CASE("PlaceFungus hits configured coverage of eligible tiles", "[worldgen][fungus]")
{
    WorldMap world(40, 30, actest::TestMapRules(), Rainfall_t::Average);
    FillLand_(world);
    BindAll_(world);
    FungusDecorationConfig_t cfg = DefaultFungus_();
    cfg.fill = 1.0f;
    std::mt19937 rng(7);
    PlaceFungus(world, cfg, 1.0f, TestFungus_(), rng);

    const int eligible = CountEligible_(world, TestFungus_()) + CountFungus_(world);
    const float coverage = static_cast<float>(CountFungus_(world)) / static_cast<float>(eligible);
    CHECK(coverage == Catch::Approx(0.30f).margin(0.05f));
}

TEST_CASE("PlaceFungus native-life multiplier scales coverage", "[worldgen][fungus]")
{
    FungusDecorationConfig_t cfg = DefaultFungus_();
    cfg.fill = 1.0f;
    cfg.poleRowChance = 0.0f;

    auto coverageFor = [&](float multiplier)
    {
        WorldMap world(40, 30, actest::TestMapRules(), Rainfall_t::Average);
        FillLand_(world);
        BindAll_(world);
        std::mt19937 rng(11);
        PlaceFungus(world, cfg, multiplier, TestFungus_(), rng);
        const int fungus = CountFungus_(world);
        const int eligible = CountEligible_(world, TestFungus_()) + fungus;
        return static_cast<float>(fungus) / static_cast<float>(eligible);
    };

    CHECK(coverageFor(0.5f) < coverageFor(1.0f));
    CHECK(coverageFor(1.0f) < coverageFor(1.5f));
}

TEST_CASE("PlaceFungus fill keeps coverage while changing patch solidity", "[worldgen][fungus]")
{
    FungusDecorationConfig_t solid = DefaultFungus_();
    solid.coverage = 0.25f;
    solid.fill = 1.0f;
    solid.poleRowChance = 0.0f;

    FungusDecorationConfig_t swiss = solid;
    swiss.fill = 0.5f;

    WorldMap solidMap(48, 36, actest::TestMapRules(), Rainfall_t::Average);
    FillLand_(solidMap);
    BindAll_(solidMap);
    std::mt19937 solidRng(19);
    PlaceFungus(solidMap, solid, 1.0f, TestFungus_(), solidRng);

    WorldMap swissMap(48, 36, actest::TestMapRules(), Rainfall_t::Average);
    FillLand_(swissMap);
    BindAll_(swissMap);
    std::mt19937 swissRng(19);
    PlaceFungus(swissMap, swiss, 1.0f, TestFungus_(), swissRng);

    const int solidCount = CountFungus_(solidMap);
    const int swissCount = CountFungus_(swissMap);
    CHECK(solidCount == Catch::Approx(swissCount).margin(solidCount * 0.15 + 1));

    // Swiss cheese should leave more fungus tiles with empty fungus-neighbors on average.
    auto emptyNeighborRate = [](WorldMap& rWorld)
    {
        int fungusTiles = 0;
        int emptyNeighborSlots = 0;
        int neighborSlots = 0;
        for (const auto& pTile : rWorld.GetTiles())
        {
            if (!pTile->HasTerrainFeature("Fungus"))
            {
                continue;
            }
            ++fungusTiles;
            ForEachNeighborInChebyshevRadius(*pTile, rWorld, 1,
                                             [&](Tile* pNeighbor, int /*distance*/)
                                             {
                                                 ++neighborSlots;
                                                 if (!pNeighbor->HasTerrainFeature("Fungus"))
                                                 {
                                                     ++emptyNeighborSlots;
                                                 }
                                             });
        }
        REQUIRE(fungusTiles > 0);
        REQUIRE(neighborSlots > 0);
        return static_cast<float>(emptyNeighborSlots) / static_cast<float>(neighborSlots);
    };

    CHECK(emptyNeighborRate(swissMap) > emptyNeighborRate(solidMap));
}

TEST_CASE("PlaceFungus cluster scale changes patch count", "[worldgen][fungus]")
{
    FungusDecorationConfig_t fine = DefaultFungus_();
    fine.coverage = 0.35f;
    fine.fill = 1.0f;
    fine.clusterScaleTiles = 3;
    fine.poleRowChance = 0.0f;

    FungusDecorationConfig_t coarse = fine;
    coarse.clusterScaleTiles = 24;

    WorldMap fineMap(48, 36, actest::TestMapRules(), Rainfall_t::Average);
    FillLand_(fineMap);
    BindAll_(fineMap);
    std::mt19937 fineRng(23);
    PlaceFungus(fineMap, fine, 1.0f, TestFungus_(), fineRng);

    WorldMap coarseMap(48, 36, actest::TestMapRules(), Rainfall_t::Average);
    FillLand_(coarseMap);
    BindAll_(coarseMap);
    std::mt19937 coarseRng(23);
    PlaceFungus(coarseMap, coarse, 1.0f, TestFungus_(), coarseRng);

    CHECK(CountFungusComponents_(fineMap) > CountFungusComponents_(coarseMap));
}

TEST_CASE("PlaceFungus pole rows roll when coverage selects none", "[worldgen][fungus]")
{
    WorldMap world(20, 11, actest::TestMapRules(), Rainfall_t::Average);
    FillLand_(world);
    BindAll_(world);
    FungusDecorationConfig_t cfg = DefaultFungus_();
    cfg.coverage = 0.0f;
    cfg.poleRowChance = 1.0f;
    std::mt19937 rng(3);
    PlaceFungus(world, cfg, 1.0f, TestFungus_(), rng);

    for (int x = 0; x < world.GetWidth(); x += 2)
    {
        CHECK(world.GetTile(x, 0)->HasTerrainFeature("Fungus"));
        CHECK(world.GetTile(x, world.GetHeight() - 1)->HasTerrainFeature("Fungus"));
    }
}

TEST_CASE("PlaceFungus stamps land and sea alike", "[worldgen][fungus]")
{
    WorldMap world(20, 11, actest::TestMapRules(), Rainfall_t::Average);
    BindAll_(world);
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation((pTile->GetX() + pTile->GetY()) % 4 == 0 ? -500 : 1000);
    }
    FungusDecorationConfig_t cfg = DefaultFungus_();
    cfg.coverage = 1.0f;
    cfg.fill = 1.0f;
    cfg.poleRowChance = 0.0f;
    std::mt19937 rng(5);
    PlaceFungus(world, cfg, 1.0f, TestFungus_(), rng);

    bool bLand = false;
    bool bSea = false;
    for (const auto& pTile : world.GetTiles())
    {
        if (!pTile->HasTerrainFeature("Fungus"))
        {
            continue;
        }
        bLand = bLand || !pTile->IsWater();
        bSea = bSea || pTile->IsWater();
    }
    CHECK(bLand);
    CHECK(bSea);
}

TEST_CASE("PlaceFungus skips tiles the fungus entry cannot occupy", "[worldgen][fungus]")
{
    ImprovementConfig_t landFungus = TestFungus_();
    landFungus.domain = ImprovementDomain_t::Land;

    WorldMap world(20, 11, actest::TestMapRules(), Rainfall_t::Average);
    BindAll_(world);
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(-500);
    }
    FungusDecorationConfig_t cfg = DefaultFungus_();
    cfg.coverage = 1.0f;
    cfg.fill = 1.0f;
    std::mt19937 rng(9);
    PlaceFungus(world, cfg, 1.0f, landFungus, rng);
    CHECK(CountFungus_(world) == 0);
}

TEST_CASE("PlaceFungus stores fungus dormant under deep ocean", "[worldgen][fungus]")
{
    WorldMap world(20, 11, actest::TestMapRules(), Rainfall_t::Average);
    BindAll_(world);
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(-3000);
    }
    FungusDecorationConfig_t cfg = DefaultFungus_();
    cfg.coverage = 1.0f;
    cfg.fill = 1.0f;
    cfg.poleRowChance = 0.0f;
    std::mt19937 rng(13);
    PlaceFungus(world, cfg, 1.0f, TestFungus_(), rng);
    REQUIRE(CountFungus_(world) > 0);
    for (const auto& pTile : world.GetTiles())
    {
        if (pTile->HasTerrainFeature("Fungus"))
        {
            CHECK(pTile->IsWater());
        }
    }
}
