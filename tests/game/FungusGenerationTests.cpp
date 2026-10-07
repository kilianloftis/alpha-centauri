#include "TestHelpers.h"
#include "game/map/FungusGeneration.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldGenDecorationConfigParser.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <queue>
#include <random>
#include <vector>

using namespace ac;

namespace
{

std::filesystem::path TempPath_(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

const ImprovementConfig_t& TestFungus_()
{
    static ImprovementRegistry registry;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        registry.LoadOccupants(actest::FixturePath("improvements.json"),
                               actest::FixturePath("terrain.json"));
        bLoaded = true;
    }
    return registry.Get("Fungus");
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
        if (pTile->HasFeature("Fungus"))
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
        if (pNeighbor->HasFeature("Fungus"))
        {
            found = true;
        }
    });
    return found;
}

// Orthogonal connected-component sizes for fungus tiles.
std::vector<int> FungusPatchSizes_(WorldMap& rWorld)
{
    std::vector<char> visited(rWorld.GetTiles().size(), 0);
    std::vector<int> sizes;

    for (const auto& pOwned : rWorld.GetTiles())
    {
        Tile* pStart = pOwned.get();
        if (!pStart->HasFeature("Fungus") || visited[static_cast<size_t>(rWorld.GetTileIndex(*pStart))])
        {
            continue;
        }

        int size = 0;
        std::queue<Tile*> q;
        q.push(pStart);
        visited[static_cast<size_t>(rWorld.GetTileIndex(*pStart))] = 1;
        while (!q.empty())
        {
            Tile* pTile = q.front();
            q.pop();
            ++size;
            ForEachOrthogonalNeighbor(*pTile, rWorld, [&](Tile* pNeighbor)
            {
                if (!pNeighbor || !pNeighbor->HasFeature("Fungus"))
                {
                    return;
                }
                const size_t n = static_cast<size_t>(rWorld.GetTileIndex(*pNeighbor));
                if (visited[n])
                {
                    return;
                }
                visited[n] = 1;
                q.push(pNeighbor);
            });
        }
        sizes.push_back(size);
    }
    return sizes;
}

} // namespace

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
  "aquifers": { "fraction": 0.01 }
})" << '\n';
    }

    WorldGenDecorationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("fungus"));
    std::filesystem::remove(path);
}

TEST_CASE("PlaceFungus covers roughly the configured fraction of viable tiles",
          "[worldgen][fungus]")
{
    WorldMap world(40, 80, actest::TestMapRules());
    FillLand_(world);

    FungusDecorationConfig_t cfg;
    cfg.fraction = 0.1f;
    cfg.minPatchTiles = 1;
    cfg.maxPatchTiles = 20;

    std::mt19937 rng(7);
    PlaceFungus(world, cfg, TestFungus_(), rng);

    const int fungus = CountFungus_(world);
    const int land = static_cast<int>(world.GetTiles().size());
    // Allow slack: patch growth can undershoot when frontiers die out.
    CHECK(fungus >= static_cast<int>(0.05f * land));
    CHECK(fungus <= static_cast<int>(0.15f * land));
}

TEST_CASE("PlaceFungus respects max_patch_tiles of 1 (no intentional growth)",
          "[worldgen][fungus]")
{
    WorldMap world(8, 16, actest::TestMapRules());
    FillLand_(world);

    FungusDecorationConfig_t cfg;
    cfg.fraction = 1.0f; // try to cover everything, but only via 1-tile patches
    cfg.minPatchTiles = 1;
    cfg.maxPatchTiles = 1;
    cfg.patchSizeSkew = 1.0f;

    std::mt19937 rng(99);
    PlaceFungus(world, cfg, TestFungus_(), rng);

    // Isolation keeps 1-tile patches from touching; growth never expands past 1.
    REQUIRE(CountFungus_(world) > 0);
    for (const auto& pTile : world.GetTiles())
    {
        if (pTile->HasFeature("Fungus"))
        {
            CHECK_FALSE(HasOrthogonalFungusNeighbor_(*pTile, world));
        }
    }
}

TEST_CASE("PlaceFungus grows contiguous multi-tile patches", "[worldgen][fungus]")
{
    WorldMap world(20, 40, actest::TestMapRules());
    FillLand_(world);

    FungusDecorationConfig_t cfg;
    cfg.fraction = 0.2f;
    cfg.minPatchTiles = 8;
    cfg.maxPatchTiles = 8;
    cfg.patchSizeSkew = 1.0f;

    std::mt19937 rng(3);
    PlaceFungus(world, cfg, TestFungus_(), rng);

    int fungusWithNeighbor = 0;
    int fungusTiles = 0;
    for (const auto& pTile : world.GetTiles())
    {
        if (!pTile->HasFeature("Fungus"))
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

TEST_CASE("PlaceFungus patch_size_skew weights toward small patches",
          "[worldgen][fungus]")
{
    WorldMap world(40, 80, actest::TestMapRules());
    FillLand_(world);

    FungusDecorationConfig_t cfg;
    cfg.fraction = 0.12f;
    cfg.minPatchTiles = 1;
    cfg.maxPatchTiles = 16;
    cfg.patchSizeSkew = 4.0f;

    std::mt19937 rng(42);
    PlaceFungus(world, cfg, TestFungus_(), rng);

    const std::vector<int> sizes = FungusPatchSizes_(world);
    REQUIRE(sizes.size() >= 10);

    int small = 0; // 1–2 tiles
    int large = 0; // near the max end
    for (int size : sizes)
    {
        if (size <= 2)
        {
            ++small;
        }
        if (size >= 10)
        {
            ++large;
        }
    }

    // Power skew + isolation ⇒ majority small; large patches are uncommon.
    CHECK(small * 2 >= static_cast<int>(sizes.size()));
    CHECK(large * 5 <= static_cast<int>(sizes.size()));
}

TEST_CASE("PlaceFungus stamps land and water from one fraction of viable tiles",
          "[worldgen][fungus]")
{
    WorldMap world(16, 32, actest::TestMapRules());
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(pTile->GetX() < 8 ? 1000 : -500);
    }

    FungusDecorationConfig_t cfg;
    cfg.fraction = 0.25f;
    cfg.minPatchTiles = 1;
    cfg.maxPatchTiles = 8;

    std::mt19937 rng(11);
    PlaceFungus(world, cfg, TestFungus_(), rng);

    int landFungus = 0;
    int waterFungus = 0;
    for (const auto& pTile : world.GetTiles())
    {
        if (!pTile->HasFeature("Fungus"))
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

    const int tiles = static_cast<int>(world.GetTiles().size());
    const int fungus = landFungus + waterFungus;
    CHECK(fungus >= static_cast<int>(0.1f * static_cast<float>(tiles)));
    CHECK(fungus <= static_cast<int>(0.4f * static_cast<float>(tiles)));
}

TEST_CASE("PlaceFungus counts only tiles the fungus entry can occupy", "[worldgen][fungus]")
{
    WorldMap world(20, 40, actest::TestMapRules());
    int land = 0;
    for (auto& pTile : world.GetTiles())
    {
        const bool bLand = pTile->GetY() < 20;
        pTile->SetElevation(bLand ? 1000 : -500);
        if (bLand)
        {
            ++land;
        }
    }

    ImprovementConfig_t landFungus = TestFungus_();
    landFungus.domain = ImprovementDomain_t::Land;

    FungusDecorationConfig_t cfg;
    cfg.fraction = 0.2f;
    cfg.minPatchTiles = 1;
    cfg.maxPatchTiles = 8;
    cfg.patchSizeSkew = 1.0f;

    std::mt19937 rng(5);
    PlaceFungus(world, cfg, landFungus, rng);

    int fungus = 0;
    for (const auto& pTile : world.GetTiles())
    {
        if (!pTile->HasFeature("Fungus"))
        {
            continue;
        }
        CHECK(pTile->IsLand());
        ++fungus;
    }
    CHECK(fungus >= static_cast<int>(0.1f * static_cast<float>(land)));
    CHECK(fungus <= static_cast<int>(0.3f * static_cast<float>(land)));
}
