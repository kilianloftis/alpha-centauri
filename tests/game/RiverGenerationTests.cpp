#include "GameFixtures.h"
#include "game/map/RiverGeneration.h"
#include "game/map/WorldGenDecorationConfigParser.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <utility>

using namespace ac;

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

void FillElevation_(WorldMap& rWorld, int elev)
{
    for (const auto& pTile : rWorld.GetTiles())
    {
        pTile->SetElevation(elev);
    }
}

constexpr int k_RiverMapWidth = 12;
constexpr int k_RiverMapHeight = 9;

} // namespace

TEST_CASE("GetRiverConnections reports orthogonal river neighbors", "[worldgen][rivers]")
{
    WorldMap world(k_RiverMapWidth, k_RiverMapHeight, actest::TestMapRules());
    FillElevation_(world, 1000);

    world.GetTile(6, 4)->SetHasRiver(true);
    world.GetTile(7, 3)->SetHasRiver(true); // N
    world.GetTile(7, 5)->SetHasRiver(true); // E
    // The tile due east on screen is a lattice diagonal and must not count.
    world.GetTile(8, 4)->SetHasRiver(true);

    const RiverConnection_t mask = GetRiverConnections(*world.GetTile(6, 4), world);
    CHECK(HasRiverConnection(mask, RiverConnection_t::North));
    CHECK(HasRiverConnection(mask, RiverConnection_t::East));
    CHECK_FALSE(HasRiverConnection(mask, RiverConnection_t::South));
    CHECK_FALSE(HasRiverConnection(mask, RiverConnection_t::West));

    CHECK(GetRiverConnections(*world.GetTile(0, 0), world) == RiverConnection_t::None);
}

TEST_CASE("WorldGenDecorationConfigParser throws when aquifers object is missing",
          "[worldgen][aquifers][parser]")
{
    const std::filesystem::path path = TempDecorationPath_("ac_decoration_no_aquifers.json");
    {
        std::ofstream file(path);
        file << R"({
  "moisture": {
    "cloudmass_peaks": 5, "cloudmass_hills": 3, "rainfall_coeff": 1,
    "hill_min_elevation_meters": 2000, "peak_min_elevation_meters": 3000
  },
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
    WorldMap world(k_RiverMapWidth, k_RiverMapHeight, actest::TestMapRules());
    FillElevation_(world, 4000);
    // Path: (8,2) -> (7,3) -> (6,4) sink
    SetLandElev_(*world.GetTile(8, 2), 3000);
    SetLandElev_(*world.GetTile(7, 3), 2000);
    SetLandElev_(*world.GetTile(6, 4), 1000);

    world.GetTile(8, 2)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(8, 2)->GetHasRiver());
    CHECK(world.GetTile(7, 3)->GetHasRiver());
    CHECK(world.GetTile(6, 4)->GetHasRiver()); // local min terminus still has river
    CHECK_FALSE(world.GetTile(6, 2)->GetHasRiver());
    CHECK_FALSE(world.GetTile(8, 4)->GetHasRiver());
}

TEST_CASE("TraceRiverFrom does not step diagonally from the aquifer", "[worldgen][rivers]")
{
    WorldMap world(k_RiverMapWidth, k_RiverMapHeight, actest::TestMapRules());
    FillElevation_(world, 3000);
    // Aquifer is a local orthogonal minimum; the tile due south on screen is a lattice
    // diagonal and much lower but must not be used.
    for (const auto& [x, y] : {std::pair{6, 4}, std::pair{7, 3}, std::pair{7, 5},
                               std::pair{5, 5}, std::pair{5, 3}})
    {
        SetLandElev_(*world.GetTile(x, y), 2000);
    }
    SetLandElev_(*world.GetTile(6, 6), 0);

    world.GetTile(6, 4)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(6, 4)->GetHasRiver());
    CHECK_FALSE(world.GetTile(6, 6)->GetHasRiver());
}

TEST_CASE("TraceRiverFrom ends on water but marks the water tile", "[worldgen][rivers]")
{
    WorldMap world(k_RiverMapWidth, k_RiverMapHeight, actest::TestMapRules());
    FillElevation_(world, 4000);
    SetLandElev_(*world.GetTile(8, 2), 2000);
    SetLandElev_(*world.GetTile(7, 3), 1000);
    world.GetTile(6, 4)->SetElevation(-100); // water
    world.GetTile(5, 5)->SetElevation(-500); // deeper — must not continue

    world.GetTile(8, 2)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(8, 2)->GetHasRiver());
    CHECK(world.GetTile(7, 3)->GetHasRiver());
    CHECK(world.GetTile(6, 4)->GetHasRiver());
    CHECK_FALSE(world.GetTile(5, 5)->GetHasRiver());
}

TEST_CASE("TraceRiverFrom prefers N before E on equal lower elevation", "[worldgen][rivers]")
{
    WorldMap world(k_RiverMapWidth, k_RiverMapHeight, actest::TestMapRules());
    FillElevation_(world, 3000);
    SetLandElev_(*world.GetTile(6, 4), 2000);
    SetLandElev_(*world.GetTile(7, 3), 1000); // N
    SetLandElev_(*world.GetTile(7, 5), 1000); // E — same elev, later in NESW

    world.GetTile(6, 4)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(6, 4)->GetHasRiver());
    CHECK(world.GetTile(7, 3)->GetHasRiver());
    CHECK_FALSE(world.GetTile(7, 5)->GetHasRiver());
}

TEST_CASE("TraceRiverFrom wraps X when flowing west", "[worldgen][rivers]")
{
    WorldMap world(k_RiverMapWidth, k_RiverMapHeight, actest::TestMapRules());
    FillElevation_(world, 4000);
    SetLandElev_(*world.GetTile(0, 4), 2000);
    SetLandElev_(*world.GetTile(k_RiverMapWidth - 1, 3), 1000); // west of (0,4) via wrap

    world.GetTile(0, 4)->SetHasAquifer(true);
    RecomputeRivers(world);

    CHECK(world.GetTile(0, 4)->GetHasRiver());
    CHECK(world.GetTile(k_RiverMapWidth - 1, 3)->GetHasRiver());
    CHECK_FALSE(world.GetTile(1, 3)->GetHasRiver());
}

TEST_CASE("RecomputeRivers clears stale path after elevation change", "[worldgen][rivers]")
{
    WorldMap world(k_RiverMapWidth, k_RiverMapHeight, actest::TestMapRules());
    FillElevation_(world, 4000);
    SetLandElev_(*world.GetTile(8, 2), 3000);
    SetLandElev_(*world.GetTile(7, 3), 2000);
    SetLandElev_(*world.GetTile(6, 4), 1000);

    world.GetTile(8, 2)->SetHasAquifer(true);
    RecomputeRivers(world);
    REQUIRE(world.GetTile(6, 4)->GetHasRiver());

    // Raise the mid tile so flow stops earlier; old terminus must lose river.
    world.GetTile(7, 3)->SetElevation(3500);
    RecomputeRivers(world);

    CHECK(world.GetTile(8, 2)->GetHasRiver());
    CHECK_FALSE(world.GetTile(7, 3)->GetHasRiver());
    CHECK_FALSE(world.GetTile(6, 4)->GetHasRiver());
}

TEST_CASE("terminates_river stops flow; terminus keeps HasRiver", "[worldgen][rivers][borehole]")
{
    actest::WorldFixture world;
    FillElevation_(world.map, 4000);
    SetLandElev_(world.At(8, 2), 4000);
    SetLandElev_(world.At(7, 3), 3000);
    SetLandElev_(world.At(6, 4), 2000);
    SetLandElev_(world.At(5, 5), 1000);
    SetLandElev_(world.At(4, 6), 500);

    world.At(8, 2).SetHasAquifer(true);
    world.ctx->AddOccupantWithEffects(world.At(6, 4), "ThermalBorehole");

    CHECK(world.At(8, 2).GetHasRiver());
    CHECK(world.At(7, 3).GetHasRiver());
    CHECK(world.At(6, 4).GetHasRiver()); // borehole terminus
    CHECK_FALSE(world.At(5, 5).GetHasRiver());
    CHECK_FALSE(world.At(4, 6).GetHasRiver());
}

TEST_CASE("Removing terminates_river improvement lets river continue", "[worldgen][rivers][borehole]")
{
    actest::WorldFixture world;
    FillElevation_(world.map, 4000);
    SetLandElev_(world.At(8, 2), 4000);
    SetLandElev_(world.At(7, 3), 3000);
    SetLandElev_(world.At(6, 4), 2000);
    SetLandElev_(world.At(5, 5), 1000);

    world.At(8, 2).SetHasAquifer(true);
    world.ctx->AddOccupantWithEffects(world.At(7, 3), "ThermalBorehole");
    REQUIRE_FALSE(world.At(6, 4).GetHasRiver());

    world.ctx->RemoveOccupantWithEffects(world.At(7, 3), "ThermalBorehole");
    CHECK(world.At(8, 2).GetHasRiver());
    CHECK(world.At(7, 3).GetHasRiver());
    CHECK(world.At(6, 4).GetHasRiver());
    CHECK(world.At(5, 5).GetHasRiver());
}

TEST_CASE("River +1 energy still applies on ThermalBorehole tile",
          "[worldgen][rivers][borehole][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    SetLandElev_(tile, 1000);
    tile.SetHasAquifer(true);
    tile.SetHasRiver(true);
    world.ctx->AddOccupantWithEffects(tile, "ThermalBorehole");

    // Borehole +6 energy, River +1; River is not in suppress_yield_sources.
    CHECK(world.ctx->ResolveTileYield(tile).effective.energy == 7);
}
