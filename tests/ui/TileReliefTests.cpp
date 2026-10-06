#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/Tile.h"
#include "ui/TileRelief.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <utility>

using namespace ac;
using Catch::Matchers::WithinAbs;

namespace
{

// A quarter tile width per 1000 m level, SMAC's full shade at a quarter level, no altitude light.
const ReliefStyle_t k_Style{0.25f, 1000.0f, 250.0f, 0.0f};

void FillMap_(actest::WorldFixture& rWorld, int elevation)
{
    for (int y = 0; y < 5; ++y)
    {
        for (int x = 0; x < 5; ++x)
        {
            rWorld.map.GetTile(x, y)->SetElevation(elevation);
        }
    }
}

TileLifts_t Lifts_(actest::WorldFixture& rWorld, int x, int y, ReliefMode_t mode)
{
    return ResolveTileLifts(*rWorld.map.GetTile(x, y), rWorld.map, mode, k_Style);
}

TileShades_t Shades_(actest::WorldFixture& rWorld, int x, int y, ReliefMode_t mode)
{
    return ResolveTileShades(*rWorld.map.GetTile(x, y), rWorld.map, mode, k_Style);
}

} // namespace

TEST_CASE("A land centre lifts by its elevation in each relief mode", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 1500);

    CHECK_THAT(Lifts_(world, 2, 2, ReliefMode_t::Smooth).center, WithinAbs(1.5f * 0.25f, 1e-4));
    CHECK_THAT(Lifts_(world, 2, 2, ReliefMode_t::Stepped).center, WithinAbs(1.0f * 0.25f, 1e-4));
    CHECK_THAT(Lifts_(world, 2, 2, ReliefMode_t::Flat).center, WithinAbs(0.0f, 1e-4));
}

TEST_CASE("A corner sits at the mean of the four tiles that share it", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 1000);
    // The N corner of (2, 2) is shared with (1, 2), (1, 1) and (2, 1).
    world.map.GetTile(1, 1)->SetElevation(3000);

    const TileLifts_t lifts = Lifts_(world, 2, 2, ReliefMode_t::Smooth);
    CHECK_THAT(lifts.north, WithinAbs((1.0f + 1.0f + 3.0f + 1.0f) / 4.0f * 0.25f, 1e-4));
    CHECK_THAT(lifts.south, WithinAbs(0.25f, 1e-4));
}

TEST_CASE("Water stays flat and pins the corners it touches to sea level", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 2000);
    world.map.GetTile(3, 2)->SetElevation(-500);

    const TileLifts_t water = Lifts_(world, 3, 2, ReliefMode_t::Smooth);
    CHECK(water.center == 0.0f);
    CHECK(water.west == 0.0f);

    // (3, 2) lies across the SE edge of (2, 2), so its E and S corners touch water.
    const TileLifts_t land = Lifts_(world, 2, 2, ReliefMode_t::Smooth);
    CHECK(land.east == 0.0f);
    CHECK(land.south == 0.0f);
    CHECK_THAT(land.north, WithinAbs(2.0f * 0.25f, 1e-4));
    CHECK_THAT(land.center, WithinAbs(2.0f * 0.25f, 1e-4));
}

TEST_CASE("Corners at the map's top edge stay at sea level", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 2000);

    const TileLifts_t lifts = Lifts_(world, 2, 0, ReliefMode_t::Smooth);
    CHECK(lifts.north == 0.0f);
    CHECK_THAT(lifts.south, WithinAbs(2.0f * 0.25f, 1e-4));
}

TEST_CASE("Flat land, water and Flat mode are unshaded", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 1500);
    world.map.GetTile(4, 4)->SetElevation(-500);

    const TileShades_t flat = Shades_(world, 2, 2, ReliefMode_t::Stepped);
    CHECK(flat.center == 0.0f);
    CHECK(flat.north == 0.0f);

    const TileShades_t water = Shades_(world, 4, 4, ReliefMode_t::Smooth);
    CHECK(water.center == 0.0f);

    world.map.GetTile(1, 1)->SetElevation(3500);
    CHECK(Shades_(world, 2, 2, ReliefMode_t::Flat).center == 0.0f);
}

TEST_CASE("A slope facing the screen's lower right is lit and the opposite darkened",
          "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 1500);

    SECTION("ground rising to the upper left faces the lower right")
    {
        // (1, 1) shares only the N corner of (2, 2), which lies at the tile's top.
        world.map.GetTile(1, 1)->SetElevation(3500);
        CHECK(Shades_(world, 2, 2, ReliefMode_t::Stepped).center < 0.0f);
    }

    SECTION("ground rising to the lower right faces the upper left")
    {
        // (3, 3) shares only the S corner, at the tile's bottom.
        world.map.GetTile(3, 3)->SetElevation(3500);
        CHECK(Shades_(world, 2, 2, ReliefMode_t::Stepped).center > 0.0f);
    }
}

TEST_CASE("Stepped lighting follows SMAC's direction-only shades", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 1500);
    // Two levels up at the N corner's diagonal tile: a = 2 quarter levels at N only, so facets
    // NW (W, N) and NE (N, E) each shade −3 · 2 / 2 = −3 and the SE and SW facets stay flat.
    world.map.GetTile(1, 1)->SetElevation(3500);

    // Centre: the mean of its four facets, (−3 − 3 + 0 + 0) / 4 palette steps.
    CHECK_THAT(Shades_(world, 2, 2, ReliefMode_t::Stepped).center, WithinAbs(-1.5f, 1e-4));
}

TEST_CASE("Smooth lighting shades gentle slopes less than steep ones", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 1500);

    world.map.GetTile(1, 1)->SetElevation(1600);
    const float gentle = Shades_(world, 2, 2, ReliefMode_t::Smooth).center;
    world.map.GetTile(1, 1)->SetElevation(3500);
    const float steep = Shades_(world, 2, 2, ReliefMode_t::Smooth).center;

    CHECK(gentle < 0.0f);
    CHECK(steep < gentle);
}

TEST_CASE("A smooth slope gets full shade once a corner rises the full-shade rise", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 1500);
    ReliefStyle_t style = k_Style;
    style.fullShadeRiseMeters = 100.0f;
    // (1, 1) shares only the N corner of (2, 2), which rises a quarter of their difference. The NW
    // and NE facets shade −3 at full shade and the centre averages them with two flat facets.
    const auto centerShade = [&](int diagonalElevation) {
        world.map.GetTile(1, 1)->SetElevation(diagonalElevation);
        return ResolveTileShades(*world.map.GetTile(2, 2), world.map, ReliefMode_t::Smooth, style)
            .center;
    };

    CHECK_THAT(centerShade(1900), WithinAbs(-1.5f, 1e-4));
    CHECK_THAT(centerShade(2700), WithinAbs(-1.5f, 1e-4));
    CHECK_THAT(centerShade(1700), WithinAbs(-0.75f, 1e-4));
}

TEST_CASE("Stepped shades match SMAC for any full-shade rise up to a quarter level",
          "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 1500);
    world.map.GetTile(1, 1)->SetElevation(2500);
    ReliefStyle_t gentle = k_Style;
    gentle.fullShadeRiseMeters = 50.0f;

    const TileShades_t smac = Shades_(world, 2, 2, ReliefMode_t::Stepped);
    const TileShades_t shades =
        ResolveTileShades(*world.map.GetTile(2, 2), world.map, ReliefMode_t::Stepped, gentle);
    CHECK_THAT(shades.center, WithinAbs(smac.center, 1e-4));
    CHECK_THAT(shades.north, WithinAbs(smac.north, 1e-4));
}

TEST_CASE("Land brightens by the altitude light for each level above sea level", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 2500);
    world.map.GetTile(4, 4)->SetElevation(-500);
    ReliefStyle_t style = k_Style;
    style.altitudeLightSteps = 1.0f;
    const auto shades = [&](int x, int y, ReliefMode_t mode) {
        return ResolveTileShades(*world.map.GetTile(x, y), world.map, mode, style);
    };

    // Level ground at 2500 m: two whole levels stepped, two and a half smooth.
    CHECK_THAT(shades(2, 2, ReliefMode_t::Stepped).center, WithinAbs(-2.0f, 1e-4));
    CHECK_THAT(shades(2, 2, ReliefMode_t::Stepped).north, WithinAbs(-2.0f, 1e-4));
    CHECK_THAT(shades(2, 2, ReliefMode_t::Smooth).center, WithinAbs(-2.5f, 1e-4));
    CHECK(shades(4, 4, ReliefMode_t::Smooth).center == 0.0f);
    CHECK(shades(2, 2, ReliefMode_t::Flat).center == 0.0f);
}

TEST_CASE("A land vertex's shade stays within SMAC's four steps either way", "[ui][relief]")
{
    actest::WorldFixture world(5, 5);
    FillMap_(world, 2500);
    ReliefStyle_t style = k_Style;
    style.altitudeLightSteps = 3.0f;

    // Two levels at three steps each would be six steps lighter.
    const TileShades_t shades =
        ResolveTileShades(*world.map.GetTile(2, 2), world.map, ReliefMode_t::Stepped, style);
    CHECK(shades.center == -4.0f);
    CHECK(shades.north == -4.0f);
}
