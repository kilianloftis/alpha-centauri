#include "TestTempDir.h"

#include "game/map/MapOccupantLoad.h"
#include "game/map/TerrainOperationRegistry.h"
#include "TestHelpers.h"
#include "game/worldgen/FungusGeneration.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/SmacFractal.h"
#include "game/map/Tile.h"
#include "game/worldgen/WorldGenDecorationConfigParser.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <random>
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

std::array<std::array<int, SmacFractal::k_LatticeSize>, SmacFractal::k_LatticeSize>
ConstantLattice_(int value)
{
    std::array<std::array<int, SmacFractal::k_LatticeSize>, SmacFractal::k_LatticeSize> lattice{};
    for (std::array<int, SmacFractal::k_LatticeSize>& rRow : lattice)
    {
        rRow.fill(value);
    }
    return lattice;
}

FungusDecorationConfig_t DefaultBands_()
{
    FungusDecorationConfig_t cfg;
    cfg.bandStarts = {20, 40, 60};
    cfg.bandWidth = 5;
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

int CountActiveFungus_(const WorldMap& rWorld)
{
    int count = 0;
    for (const auto& pTile : rWorld.GetTiles())
    {
        if (pTile->HasFeature("Fungus"))
        {
            ++count;
        }
    }
    return count;
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

TEST_CASE("WorldGenDecorationConfigParser requires fungus band keys",
          "[worldgen][fungus][parser]")
{
    const WorldGenDecorationConfig_t cfg =
        WorldGenDecorationConfigParser{}.ParseConfig(
            actest::FixturePath("worldGen/decoration.json"));
    CHECK(cfg.fungus.bandStarts == std::vector<int>({20, 40, 60}));
    CHECK(cfg.fungus.bandWidth == 5);
    CHECK(cfg.fungus.poleRowChance == Catch::Approx(0.5f));
}

TEST_CASE("PlaceFungus constant lattices cover tiles by band widening", "[worldgen][fungus]")
{
    WorldMap world(16, 16, actest::TestMapRules(), Rainfall_t::Average);
    FillLand_(world);
    const FungusDecorationConfig_t cfg = DefaultBands_();
    std::mt19937 rng(1);

    SECTION("lattice 4 covers every tile at widening 0 and none at -1")
    {
        const SmacFractal fractal(ConstantLattice_(4));
        PlaceFungus(world, cfg, 0, TestFungus_(), fractal, rng);
        CHECK(CountFungus_(world) == static_cast<int>(world.GetTiles().size()));

        WorldMap none(16, 16, actest::TestMapRules(), Rainfall_t::Average);
        FillLand_(none);
        PlaceFungus(none, cfg, -1, TestFungus_(), fractal, rng);
        CHECK(CountFungus_(none) == 0);
    }

    SECTION("lattice 7 covers every tile at all three widenings")
    {
        const SmacFractal fractal(ConstantLattice_(7));
        for (const int widening : {-1, 0, 1})
        {
            WorldMap map(16, 16, actest::TestMapRules(), Rainfall_t::Average);
            FillLand_(map);
            PlaceFungus(map, cfg, widening, TestFungus_(), fractal, rng);
            CHECK(CountFungus_(map) == static_cast<int>(map.GetTiles().size()));
        }
    }

    SECTION("lattice 11 covers every tile only at widening +1")
    {
        const SmacFractal fractal(ConstantLattice_(11));
        for (const int widening : {-1, 0})
        {
            WorldMap map(16, 16, actest::TestMapRules(), Rainfall_t::Average);
            FillLand_(map);
            PlaceFungus(map, cfg, widening, TestFungus_(), fractal, rng);
            CHECK(CountFungus_(map) == 0);
        }
        WorldMap abundant(16, 16, actest::TestMapRules(), Rainfall_t::Average);
        FillLand_(abundant);
        PlaceFungus(abundant, cfg, 1, TestFungus_(), fractal, rng);
        CHECK(CountFungus_(abundant) == static_cast<int>(abundant.GetTiles().size()));
    }

    SECTION("lattice 3 covers none")
    {
        const SmacFractal fractal(ConstantLattice_(3));
        PlaceFungus(world, cfg, 1, TestFungus_(), fractal, rng);
        CHECK(CountFungus_(world) == 0);
    }
}

TEST_CASE("PlaceFungus pole rows roll when every band misses", "[worldgen][fungus]")
{
    const SmacFractal fractal(ConstantLattice_(3));
    FungusDecorationConfig_t cfg = DefaultBands_();
    std::mt19937 rng(2);

    SECTION("chance 1 covers exactly the first and last rows")
    {
        cfg.poleRowChance = 1.0f;
        WorldMap world(20, 12, actest::TestMapRules(), Rainfall_t::Average);
        FillLand_(world);
        PlaceFungus(world, cfg, 0, TestFungus_(), fractal, rng);

        for (const auto& pTile : world.GetTiles())
        {
            const bool bPole = pTile->GetY() == 0 || pTile->GetY() == world.GetHeight() - 1;
            CHECK(pTile->HasTerrainFeature("Fungus") == bPole);
        }
    }

    SECTION("chance 0 covers nothing")
    {
        cfg.poleRowChance = 0.0f;
        WorldMap world(20, 12, actest::TestMapRules(), Rainfall_t::Average);
        FillLand_(world);
        PlaceFungus(world, cfg, 0, TestFungus_(), fractal, rng);
        CHECK(CountFungus_(world) == 0);
    }
}

TEST_CASE("PlaceFungus follows each tile's own coordinates", "[worldgen][fungus]")
{
    auto lattice = ConstantLattice_(0);
    // A single hot cell so Value varies across the map.
    lattice[2][3] = 15;
    const SmacFractal fractal(lattice);
    FungusDecorationConfig_t cfg = DefaultBands_();
    cfg.poleRowChance = 0.0f;

    WorldMap world(32, 24, actest::TestMapRules(), Rainfall_t::Average);
    FillLand_(world);
    std::mt19937 rng(3);
    PlaceFungus(world, cfg, 0, TestFungus_(), fractal, rng);

    for (const auto& pTile : world.GetTiles())
    {
        const int value = fractal.Value(pTile->GetX(), pTile->GetY(), 4);
        bool bInBand = false;
        for (const int start : cfg.bandStarts)
        {
            if (value >= start && value <= start + cfg.bandWidth)
            {
                bInBand = true;
            }
        }
        CHECK(pTile->HasTerrainFeature("Fungus") == bInBand);
    }
}

TEST_CASE("PlaceFungus stamps land and sea alike", "[worldgen][fungus]")
{
    WorldMap world(16, 32, actest::TestMapRules(), Rainfall_t::Average);
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(pTile->GetX() < 8 ? 1000 : -500);
    }

    const SmacFractal fractal(ConstantLattice_(7));
    FungusDecorationConfig_t cfg = DefaultBands_();
    std::mt19937 rng(11);
    PlaceFungus(world, cfg, 0, TestFungus_(), fractal, rng);

    int landFungus = 0;
    int waterFungus = 0;
    for (const auto& pTile : world.GetTiles())
    {
        if (!pTile->HasTerrainFeature("Fungus"))
        {
            continue;
        }
        if (pTile->IsLand())
        {
            ++landFungus;
        }
        if (pTile->IsWater())
        {
            ++waterFungus;
        }
    }
    CHECK(landFungus > 0);
    CHECK(waterFungus > 0);
    CHECK(CountFungus_(world) == static_cast<int>(world.GetTiles().size()));
}

TEST_CASE("PlaceFungus skips tiles the fungus entry cannot occupy", "[worldgen][fungus]")
{
    WorldMap world(20, 40, actest::TestMapRules(), Rainfall_t::Average);
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(pTile->GetY() < 20 ? 1000 : -500);
    }

    ImprovementConfig_t landFungus = TestFungus_();
    landFungus.domain = ImprovementDomain_t::Land;

    const SmacFractal fractal(ConstantLattice_(7));
    FungusDecorationConfig_t cfg = DefaultBands_();
    std::mt19937 rng(5);
    PlaceFungus(world, cfg, 0, landFungus, fractal, rng);

    for (const auto& pTile : world.GetTiles())
    {
        if (pTile->HasTerrainFeature("Fungus"))
        {
            CHECK(pTile->IsLand());
        }
        else
        {
            CHECK(pTile->IsWater());
        }
    }
}

TEST_CASE("PlaceFungus stores fungus dormant under deep ocean", "[worldgen][fungus]")
{
    WorldMap world(12, 12, actest::TestMapRules(), Rainfall_t::Average);
    BindAll_(world);
    const int shelf = actest::TestMapRules().oceanShelfMeters;
    for (auto& pTile : world.GetTiles())
    {
        // Deep ocean: below the shelf threshold.
        pTile->SetElevation(shelf - 500);
    }

    const SmacFractal fractal(ConstantLattice_(7));
    FungusDecorationConfig_t cfg = DefaultBands_();
    std::mt19937 rng(9);
    PlaceFungus(world, cfg, 0, TestFungus_(), fractal, rng);

    CHECK(CountFungus_(world) == static_cast<int>(world.GetTiles().size()));
    CHECK(CountActiveFungus_(world) == 0);
}

TEST_CASE("PlaceFungus density tracks native-life widening on a standard map",
          "[worldgen][fungus][density]")
{
    FungusDecorationConfig_t cfg = DefaultBands_();
    cfg.poleRowChance = 0.5f;
    const std::vector<unsigned> seeds = {1u, 2u, 3u, 5u, 8u};

    auto coverage = [&](int widening) {
        double total = 0.0;
        for (const unsigned seed : seeds)
        {
            WorldMap world(112, 56, actest::TestMapRules(), Rainfall_t::Average);
            FillLand_(world);
            std::mt19937 rng(seed);
            const SmacFractal fractal(rng);
            PlaceFungus(world, cfg, widening, TestFungus_(), fractal, rng);
            const double share = static_cast<double>(CountFungus_(world))
                                 / static_cast<double>(world.GetTiles().size());
            total += share;
        }
        return total / static_cast<double>(seeds.size());
    };

    CHECK(coverage(-1) == Catch::Approx(0.15).margin(0.03));
    CHECK(coverage(0) == Catch::Approx(0.30).margin(0.03));
    CHECK(coverage(1) == Catch::Approx(0.45).margin(0.03));
}
