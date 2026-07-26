#include "game/map/FungusGeneration.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldGenDecorationConfigParser.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <random>

using namespace ac;
using Catch::Approx;

namespace
{

std::filesystem::path TempPath_(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

void FillLand_(WorldMap& rWorld)
{
    for (auto& pTile : rWorld.GetTiles())
    {
        pTile->SetElevation(1000);
    }
}

int CountFungus_(const WorldMap& rWorld)
{
    int count = 0;
    for (const auto& pTile : rWorld.GetTiles())
    {
        if (pTile->GetHasFungus())
        {
            ++count;
        }
    }
    return count;
}

bool HasOrthogonalFungusNeighbor_(const Tile& rTile, const WorldMap& rWorld)
{
    bool found = false;
    ForEachOrthogonalNeighbor(rTile, rWorld, [&](const Tile* pNeighbor)
    {
        if (pNeighbor->GetHasFungus())
        {
            found = true;
        }
    });
    return found;
}

} // namespace

TEST_CASE("WorldGenDecorationConfigParser loads fungus knobs from decoration.json",
          "[worldgen][fungus][parser]")
{
    WorldGenDecorationConfigParser parser;
    const WorldGenDecorationConfig_t config =
        parser.ParseConfig(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/worldGen/decoration.json");

    CHECK(config.fungus.landFraction == Approx(0.08f));
    CHECK(config.fungus.waterFraction == Approx(0.02f));
    CHECK(config.fungus.minPatchTiles == 1);
    CHECK(config.fungus.maxPatchTiles == 48);
}

TEST_CASE("WorldGenDecorationConfigParser throws when fungus object is missing",
          "[worldgen][fungus][parser]")
{
    const std::filesystem::path path = TempPath_("ac_decoration_no_fungus.json");
    {
        std::ofstream file(path);
        file << R"({
  "rockiness": {
    "low": { "flat": 0.5, "rolling": 0.3, "rocky": 0.2 },
    "average": { "flat": 0.5, "rolling": 0.3, "rocky": 0.2 },
    "high": { "flat": 0.5, "rolling": 0.3, "rocky": 0.2 }
  },
  "aquifers": { "land_fraction": 0.01 }
})" << '\n';
    }

    WorldGenDecorationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("fungus"));
    std::filesystem::remove(path);
}

TEST_CASE("PlaceFungus covers roughly the configured land fraction", "[worldgen][fungus]")
{
    WorldMap world(40, 40);
    FillLand_(world);

    FungusDecorationConfig_t cfg;
    cfg.landFraction = 0.1f;
    cfg.waterFraction = 0.0f;
    cfg.minPatchTiles = 1;
    cfg.maxPatchTiles = 20;

    std::mt19937 rng(7);
    PlaceFungus(world, cfg, rng);

    const int fungus = CountFungus_(world);
    const int land = world.GetWidth() * world.GetHeight();
    // Allow slack: patch growth can undershoot when frontiers die out.
    CHECK(fungus >= static_cast<int>(0.05f * land));
    CHECK(fungus <= static_cast<int>(0.15f * land));
}

TEST_CASE("PlaceFungus respects max_patch_tiles of 1 (no intentional growth)",
          "[worldgen][fungus]")
{
    WorldMap world(8, 8);
    FillLand_(world);

    FungusDecorationConfig_t cfg;
    cfg.landFraction = 1.0f; // try to cover everything, but only via 1-tile patches
    cfg.minPatchTiles = 1;
    cfg.maxPatchTiles = 1;

    std::mt19937 rng(99);
    PlaceFungus(world, cfg, rng);

    // Every tile may be fungus (adjacent seeds), but growth never expands a patch.
    CHECK(CountFungus_(world) == world.GetWidth() * world.GetHeight());
}

TEST_CASE("PlaceFungus grows contiguous multi-tile patches", "[worldgen][fungus]")
{
    WorldMap world(20, 20);
    FillLand_(world);

    FungusDecorationConfig_t cfg;
    cfg.landFraction = 0.2f;
    cfg.minPatchTiles = 8;
    cfg.maxPatchTiles = 8;

    std::mt19937 rng(3);
    PlaceFungus(world, cfg, rng);

    int fungusWithNeighbor = 0;
    int fungusTiles = 0;
    for (const auto& pTile : world.GetTiles())
    {
        if (!pTile->GetHasFungus())
        {
            continue;
        }
        ++fungusTiles;
        if (HasOrthogonalFungusNeighbor_(*pTile, world))
        {
            ++fungusWithNeighbor;
        }
    }
    REQUIRE(fungusTiles >= 8);
    // Interior/edge of an 8-tile patch: most tiles should touch another fungus tile.
    CHECK(fungusWithNeighbor >= fungusTiles - 2);
}

TEST_CASE("PlaceFungus water_fraction only stamps water tiles", "[worldgen][fungus]")
{
    WorldMap world(16, 16);
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(-500);
    }

    FungusDecorationConfig_t cfg;
    cfg.landFraction = 0.0f;
    cfg.waterFraction = 0.15f;
    cfg.minPatchTiles = 2;
    cfg.maxPatchTiles = 10;

    std::mt19937 rng(11);
    PlaceFungus(world, cfg, rng);

    for (const auto& pTile : world.GetTiles())
    {
        if (pTile->GetHasFungus())
        {
            CHECK(pTile->IsWater());
        }
    }
    CHECK(CountFungus_(world) > 0);
}
