#include "GameFixtures.h"
#include "game/map/RiverGeneration.h"
#include "game/map/WorldGenDecorationConfigParser.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

using namespace ac;
using Catch::Approx;

namespace
{

std::filesystem::path TempDecorationPath_(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

void SetLandElev_(Tile& rTile, int elev)
{
    rTile.SetElevation(elev);
}

} // namespace

TEST_CASE("GetRiverConnections reports orthogonal river neighbors", "[worldgen][rivers]")
{
    WorldMap world(3, 3);
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            world.GetTile(x, y)->SetElevation(1000);
        }
    }

    world.GetTile(1, 1)->SetHasRiver(true);
    world.GetTile(1, 0)->SetHasRiver(true); // N
    world.GetTile(2, 1)->SetHasRiver(true); // E
    // Diagonal must not count
    world.GetTile(2, 0)->SetHasRiver(true);

    const RiverConnection_t mask = GetRiverConnections(*world.GetTile(1, 1), world);
    CHECK(HasRiverConnection(mask, RiverConnection_t::North));
    CHECK(HasRiverConnection(mask, RiverConnection_t::East));
    CHECK_FALSE(HasRiverConnection(mask, RiverConnection_t::South));
    CHECK_FALSE(HasRiverConnection(mask, RiverConnection_t::West));

    CHECK(GetRiverConnections(*world.GetTile(0, 0), world) == RiverConnection_t::None);
}

TEST_CASE("WorldGenDecorationConfigParser loads aquifers from decoration.json",
          "[worldgen][aquifers][parser]")
{
    WorldGenDecorationConfigParser parser;
    const WorldGenDecorationConfig_t config =
        parser.ParseConfig(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/worldGen/decoration.json");
    CHECK(config.aquifers.landFraction == Approx(0.002f));
}

TEST_CASE("WorldGenDecorationConfigParser throws when aquifers object is missing",
          "[worldgen][aquifers][parser]")
{
    const std::filesystem::path path = TempDecorationPath_("ac_decoration_no_aquifers.json");
    {
        std::ofstream file(path);
        file << R"({
  "rockiness": {
    "low": { "flat": 0.5, "rolling": 0.3, "rocky": 0.2 },
    "average": { "flat": 0.5, "rolling": 0.3, "rocky": 0.2 },
    "high": { "flat": 0.5, "rolling": 0.3, "rocky": 0.2 }
  }
})" << '\n';
    }

    WorldGenDecorationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("aquifers"));
    std::filesystem::remove(path);
}

TEST_CASE("TraceRiverFrom follows orthogonal downhill and keeps terminus HasRiver",
          "[worldgen][rivers]")
{
    WorldMap world(5, 5);
    // Path: (2,0) -> (2,1) -> (2,2) sink
    SetLandElev_(*world.GetTile(2, 0), 3000);
    SetLandElev_(*world.GetTile(2, 1), 2000);
    SetLandElev_(*world.GetTile(2, 2), 1000);
    // Distractors / higher neighbors
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            if (x == 2 && y <= 2)
            {
                continue;
            }
            SetLandElev_(*world.GetTile(x, y), 4000);
        }
    }

    world.GetTile(2, 0)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(2, 0)->GetHasRiver());
    CHECK(world.GetTile(2, 1)->GetHasRiver());
    CHECK(world.GetTile(2, 2)->GetHasRiver()); // local min terminus still has river
    CHECK_FALSE(world.GetTile(1, 1)->GetHasRiver());
    CHECK_FALSE(world.GetTile(3, 1)->GetHasRiver());
}

TEST_CASE("TraceRiverFrom does not step diagonally from the aquifer", "[worldgen][rivers]")
{
    WorldMap world(3, 3);
    // Aquifer is a local orthogonal minimum; diagonal (1,1) is much lower but must not be used.
    SetLandElev_(*world.GetTile(0, 0), 2000);
    SetLandElev_(*world.GetTile(1, 0), 2000);
    SetLandElev_(*world.GetTile(0, 1), 2000);
    SetLandElev_(*world.GetTile(1, 1), 0);
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            if ((x == 0 && y == 0) || (x == 1 && y == 0) || (x == 0 && y == 1)
                || (x == 1 && y == 1))
            {
                continue;
            }
            SetLandElev_(*world.GetTile(x, y), 3000);
        }
    }

    world.GetTile(0, 0)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(0, 0)->GetHasRiver());
    CHECK_FALSE(world.GetTile(1, 1)->GetHasRiver());
}

TEST_CASE("TraceRiverFrom ends on water but marks the water tile", "[worldgen][rivers]")
{
    // Column path (avoids X-wrap stealing the downhill step).
    WorldMap world(3, 4);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            SetLandElev_(*world.GetTile(x, y), 4000);
        }
    }
    SetLandElev_(*world.GetTile(1, 0), 2000);
    SetLandElev_(*world.GetTile(1, 1), 1000);
    world.GetTile(1, 2)->SetElevation(-100); // water
    world.GetTile(1, 3)->SetElevation(-500); // deeper — must not continue

    world.GetTile(1, 0)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(1, 0)->GetHasRiver());
    CHECK(world.GetTile(1, 1)->GetHasRiver());
    CHECK(world.GetTile(1, 2)->GetHasRiver());
    CHECK_FALSE(world.GetTile(1, 3)->GetHasRiver());
}

TEST_CASE("TraceRiverFrom prefers N before E on equal lower elevation", "[worldgen][rivers]")
{
    WorldMap world(3, 3);
    SetLandElev_(*world.GetTile(1, 1), 2000);
    SetLandElev_(*world.GetTile(1, 0), 1000); // N
    SetLandElev_(*world.GetTile(2, 1), 1000); // E — same elev, later in NESW
    SetLandElev_(*world.GetTile(1, 2), 3000);
    SetLandElev_(*world.GetTile(0, 1), 3000);
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            if ((x == 1 && y == 1) || (x == 1 && y == 0) || (x == 2 && y == 1)
                || (x == 1 && y == 2) || (x == 0 && y == 1))
            {
                continue;
            }
            SetLandElev_(*world.GetTile(x, y), 3000);
        }
    }

    world.GetTile(1, 1)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(1, 1)->GetHasRiver());
    CHECK(world.GetTile(1, 0)->GetHasRiver());
    CHECK_FALSE(world.GetTile(2, 1)->GetHasRiver());
}

TEST_CASE("TraceRiverFrom wraps X when flowing west", "[worldgen][rivers]")
{
    WorldMap world(4, 2);
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            SetLandElev_(*world.GetTile(x, y), 4000);
        }
    }
    SetLandElev_(*world.GetTile(0, 0), 2000);
    SetLandElev_(*world.GetTile(3, 0), 1000); // west of 0 via wrap

    world.GetTile(0, 0)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(0, 0)->GetHasRiver());
    CHECK(world.GetTile(3, 0)->GetHasRiver());
    CHECK_FALSE(world.GetTile(1, 0)->GetHasRiver());
}

TEST_CASE("RecomputeRivers clears stale path after elevation change", "[worldgen][rivers]")
{
    WorldMap world(3, 4);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            SetLandElev_(*world.GetTile(x, y), 4000);
        }
    }
    SetLandElev_(*world.GetTile(1, 0), 3000);
    SetLandElev_(*world.GetTile(1, 1), 2000);
    SetLandElev_(*world.GetTile(1, 2), 1000);

    world.GetTile(1, 0)->SetHasAquifer(true);
    RecomputeRivers(world);
    REQUIRE(world.GetTile(1, 2)->GetHasRiver());

    // Raise the mid tile so flow stops earlier; old terminus must lose river.
    world.GetTile(1, 1)->SetElevation(3500);
    RecomputeRivers(world);

    CHECK(world.GetTile(1, 0)->GetHasRiver());
    CHECK_FALSE(world.GetTile(1, 1)->GetHasRiver());
    CHECK_FALSE(world.GetTile(1, 2)->GetHasRiver());
}

TEST_CASE("terminates_river stops flow; terminus keeps HasRiver", "[worldgen][rivers][borehole]")
{
    actest::WorldFixture world(3, 5);
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            SetLandElev_(world.At(x, y), 4000);
        }
    }
    SetLandElev_(world.At(1, 0), 4000);
    SetLandElev_(world.At(1, 1), 3000);
    SetLandElev_(world.At(1, 2), 2000);
    SetLandElev_(world.At(1, 3), 1000);
    SetLandElev_(world.At(1, 4), 500);

    world.At(1, 0).SetHasAquifer(true);
    world.ctx->AddImprovementWithEffects(world.At(1, 2), "ThermalBorehole");

    CHECK(world.At(1, 0).GetHasRiver());
    CHECK(world.At(1, 1).GetHasRiver());
    CHECK(world.At(1, 2).GetHasRiver()); // borehole terminus
    CHECK_FALSE(world.At(1, 3).GetHasRiver());
    CHECK_FALSE(world.At(1, 4).GetHasRiver());
}

TEST_CASE("Removing terminates_river improvement lets river continue", "[worldgen][rivers][borehole]")
{
    actest::WorldFixture world(3, 4);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            SetLandElev_(world.At(x, y), 4000);
        }
    }
    SetLandElev_(world.At(1, 0), 4000);
    SetLandElev_(world.At(1, 1), 3000);
    SetLandElev_(world.At(1, 2), 2000);
    SetLandElev_(world.At(1, 3), 1000);

    world.At(1, 0).SetHasAquifer(true);
    world.ctx->AddImprovementWithEffects(world.At(1, 1), "ThermalBorehole");
    REQUIRE_FALSE(world.At(1, 2).GetHasRiver());

    world.ctx->RemoveImprovementWithEffects(world.At(1, 1), "ThermalBorehole");
    CHECK(world.At(1, 0).GetHasRiver());
    CHECK(world.At(1, 1).GetHasRiver());
    CHECK(world.At(1, 2).GetHasRiver());
    CHECK(world.At(1, 3).GetHasRiver());
}

TEST_CASE("River +1 energy still applies on ThermalBorehole tile",
          "[worldgen][rivers][borehole][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    SetLandElev_(tile, 1000);
    tile.SetHasAquifer(true);
    tile.SetHasRiver(true);
    world.ctx->AddImprovementWithEffects(tile, "ThermalBorehole");

    // Borehole +6 energy, River +1; River is not in suppress_yield_sources.
    CHECK(world.ctx->ResolveTileYield(tile).effective.energy == 7);
}
