// Tests for TileEffectsContext: area (aura) collection, tile yield resolution, defense
// multipliers, moisture recomputation, and improvement add/remove with effects.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using Catch::Approx;

TEST_CASE("CollectAreaEffects: radius improvements reach exactly their Manhattan radius",
          "[effects][tile][aura]")
{
    actest::WorldFixture world;
    // Sensor (radius 2) at the center.
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "Sensor");

    SECTION("distance 1 and 2 are covered")
    {
        CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(5, 4)) == Approx(1.25));
        CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(6, 4)) == Approx(1.25));
        CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(5, 5)) == Approx(1.25)); // dist 2 diagonal
    }

    SECTION("distance 3 is not covered")
    {
        CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(7, 4)) == Approx(1.0));
        CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(6, 6)) == Approx(1.0)); // dist 4
    }

    SECTION("the sensor's own tile is covered")
    {
        CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(4, 4)) == Approx(1.25));
    }
}

TEST_CASE("CollectAreaEffects: radius-0 improvements never reach neighbors", "[effects][tile][aura]")
{
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "Bunker");

    CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(4, 4)) == Approx(1.5));
    CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(5, 4)) == Approx(1.0));
}

TEST_CASE("ResolveTileDefenseMultiplier: terrain and improvement bonuses combine arithmetically",
          "[effects][tile][defense]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);

    // Base multiplier for a featureless tile is 1.0.
    CHECK(world.ctx->ResolveTileDefenseMultiplier(tile) == Approx(1.0));

    tile.SetRockiness(Rockiness::Rocky); // +25%
    CHECK(world.ctx->ResolveTileDefenseMultiplier(tile) == Approx(1.25));

    world.ctx->AddImprovementWithEffects(tile, "Bunker"); // +50%
    // Arithmetic combination: 1 + 0.25 + 0.50, not 1.25 * 1.5.
    CHECK(world.ctx->ResolveTileDefenseMultiplier(tile) == Approx(1.75));

    world.ctx->AddImprovementWithEffects(world.At(5, 4), "Sensor"); // +25% aura
    CHECK(world.ctx->ResolveTileDefenseMultiplier(tile) == Approx(2.0));
}

TEST_CASE("ResolveTileYield: energy is seeded from elevation, other resources start at zero",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);

    SECTION("featureless tile at sea level yields nothing")
    {
        const TileResources_t yield = world.ctx->ResolveTileYield(tile);
        CHECK(yield.nutrients == 0);
        CHECK(yield.minerals == 0);
        CHECK(yield.energy == 0);
    }

    SECTION("elevation seeds energy at 1 per 1000m")
    {
        tile.SetElevation(1000);
        CHECK(world.ctx->ResolveTileYield(tile).energy == 1);
        tile.SetElevation(2500);
        CHECK(world.ctx->ResolveTileYield(tile).energy == 2);
    }

    SECTION("negative elevation seeds zero, not negative energy")
    {
        tile.SetElevation(-2000);
        CHECK(world.ctx->ResolveTileYield(tile).energy == 0);
    }
}

TEST_CASE("ResolveTileYield: each resource resolves from the matching StatId (field-order sanity)",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);

    tile.SetElevation(1000);   // energy seed 1
    tile.SetHasRiver(true);    // +1 energy
    world.ctx->AddImprovementWithEffects(tile, "Farm"); // +1 nutrients
    world.ctx->AddImprovementWithEffects(tile, "Mine"); // +2 minerals

    const TileResources_t yield = world.ctx->ResolveTileYield(tile);
    CHECK(yield.nutrients == 1);
    CHECK(yield.minerals == 2);
    CHECK(yield.energy == 2);
}

TEST_CASE("ResolveTileYield: terrain classification contributes through the same registry ids",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    tile.SetMoisture(Moisture::Wet);       // +2 nutrients
    tile.SetRockiness(Rockiness::Rolling); // +1 mineral

    const TileResources_t yield = world.ctx->ResolveTileYield(tile);
    CHECK(yield.nutrients == 2);
    CHECK(yield.minerals == 1);
}

TEST_CASE("ResolveTileYield: a Mirror's energy aura reaches nearby tiles", "[effects][tile][yield][aura]")
{
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "Mirror");

    CHECK(world.ctx->ResolveTileYield(world.At(6, 4)).energy == 1); // distance 2
    CHECK(world.ctx->ResolveTileYield(world.At(7, 4)).energy == 0); // distance 3

    // Two mirrors in range stack.
    world.ctx->AddImprovementWithEffects(world.At(6, 5), "Mirror");
    CHECK(world.ctx->ResolveTileYield(world.At(6, 4)).energy == 2);
}

TEST_CASE("ResolveTileYield with base effects: selector-carrying modifiers apply per matching tile",
          "[effects][tile][yield][selector]")
{
    actest::WorldFixture world;
    Tile& farmTile = world.At(4, 4);
    Tile& plainTile = world.At(5, 4);
    world.ctx->AddImprovementWithEffects(farmTile, "Farm");

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{{
        actest::Active(pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Farm")), "farm_booster"),
    }};

    // Farm tile: 1 (Farm) + 1 (booster). Plain tile: unaffected.
    CHECK(world.ctx->ResolveTileYield(farmTile, false, baseEffects).nutrients == 2);
    CHECK(world.ctx->ResolveTileYield(plainTile, false, baseEffects).nutrients == 0);
}

TEST_CASE("ResolveTileYield with base effects: BaseTile selector applies only to the base center tile",
          "[effects][tile][yield][selector]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{{
        actest::Active(pool.StatMod(StatId::Energy, 2.0, ModifierOp::Add, EffectScope_t::ThisBase,
                                    actest::BaseTileSelector()), "center_booster"),
    }};

    CHECK(world.ctx->ResolveTileYield(tile, true, baseEffects).energy == 2);
    CHECK(world.ctx->ResolveTileYield(tile, false, baseEffects).energy == 0);
}

TEST_CASE("ResolveTileYield with base effects: flat (non-selector) modifiers are NOT applied per tile",
          "[effects][tile][yield][selector]")
{
    // Flat base bonuses resolve once at the base level (FilterFlatByStatId); applying them
    // per worked tile would multiply them by the number of workers.
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{{
        actest::Active(pool.StatMod(StatId::Nutrients, 2.0, ModifierOp::Add, EffectScope_t::ThisBase),
                       "flat_nutrient"),
    }};

    CHECK(world.ctx->ResolveTileYield(tile, true, baseEffects).nutrients == 0);
    CHECK(world.ctx->ResolveTileYield(tile, false, baseEffects).nutrients == 0);
}

TEST_CASE("ResolveTileYield: percentage modifiers scale a tile's own yield", "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    // Base moisture must be set too: AddImprovementWithEffects triggers RecomputeMoisture,
    // which re-derives the effective value from the base value (world-gen sets both).
    tile.SetBaseMoisture(Moisture::Wet);
    tile.SetMoisture(Moisture::Wet); // +2 nutrients
    world.ctx->AddImprovementWithEffects(tile, "Farm"); // +1 nutrients

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{{
        actest::Active(pool.StatMod(StatId::Nutrients, 50.0, ModifierOp::AddPercent, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Farm")), "gene_splicer"),
    }};

    // (2 + 1) * 1.5 = 4.5, truncated to 4 by the int cast.
    CHECK(world.ctx->ResolveTileYield(tile, false, baseEffects).nutrients == 4);
}

TEST_CASE("RecomputeMoisture: Condenser aura raises effective moisture, derived fresh from base moisture",
          "[effects][tile][moisture]")
{
    actest::WorldFixture world;
    Tile& dryTile = world.At(4, 4);
    dryTile.SetBaseMoisture(Moisture::Arid);
    dryTile.SetMoisture(Moisture::Arid);

    // Condenser (radius 1) next door.
    world.ctx->AddImprovementWithEffects(world.At(5, 4), "Condenser");

    CHECK(dryTile.GetMoisture() == Moisture::Moist);     // effective value shifted
    CHECK(dryTile.GetBaseMoisture() == Moisture::Arid);  // terrain truth untouched

    SECTION("recompute is idempotent")
    {
        world.ctx->RecomputeMoisture(dryTile);
        world.ctx->RecomputeMoisture(dryTile);
        CHECK(dryTile.GetMoisture() == Moisture::Moist);
    }

    SECTION("overlapping condensers stack and clamp at Wet")
    {
        world.ctx->AddImprovementWithEffects(world.At(3, 4), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture::Wet); // Arid + 2

        world.ctx->AddImprovementWithEffects(world.At(4, 5), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture::Wet); // clamped, no overflow
    }

    SECTION("removal reverts cleanly")
    {
        world.ctx->RemoveImprovementWithEffects(world.At(5, 4), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture::Arid);
    }

    SECTION("removal with another condenser still in range keeps the remaining bonus")
    {
        world.ctx->AddImprovementWithEffects(world.At(3, 4), "Condenser");
        world.ctx->RemoveImprovementWithEffects(world.At(5, 4), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture::Moist);
    }
}

TEST_CASE("RecomputeMoisture: the moisture shift feeds back into tile yield", "[effects][tile][moisture]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    tile.SetBaseMoisture(Moisture::Arid);
    tile.SetMoisture(Moisture::Arid);
    CHECK(world.ctx->ResolveTileYield(tile).nutrients == 0);

    world.ctx->AddImprovementWithEffects(tile, "Condenser");
    // Now effectively Moist: +1 nutrients through the Moist terrain feature.
    CHECK(world.ctx->ResolveTileYield(tile).nutrients == 1);
}

TEST_CASE("AddImprovementWithEffects: unknown improvement ids are rejected without crashing",
          "[effects][tile]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    world.ctx->AddImprovementWithEffects(tile, "OrbitalLaser");
    CHECK(tile.GetImprovements().empty());
    world.ctx->RemoveImprovementWithEffects(tile, "OrbitalLaser"); // also a safe no-op
}

TEST_CASE("Aura effects at the map edge are collected without crashing", "[effects][tile][aura]")
{
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(0, 0), "Sensor");
    CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(0, 0)) == Approx(1.25));
    CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(1, 0)) == Approx(1.25));
    CHECK(world.ctx->ResolveTileYield(world.At(0, 1)).nutrients == 0);
}

TEST_CASE("CanBuildImprovement: excludes-list features block construction", "[effects][tile]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    const ImprovementConfig_t* pFarm = world.improvements.Find("Farm");
    REQUIRE(pFarm != nullptr);

    CHECK(CanBuildImprovement(tile, *pFarm));
    tile.SetRockiness(Rockiness::Rocky);
    CHECK_FALSE(CanBuildImprovement(tile, *pFarm)); // Farm excludes Rocky
    tile.SetRockiness(Rockiness::Rolling);
    CHECK(CanBuildImprovement(tile, *pFarm));
}

TEST_CASE("Per-effect radius: an effect's own radius grants reach beyond the improvement's",
          "[effects][tile][aura][radius]")
{
    // EffectRadiusBeacon: improvement radius 0, but its energy effect declares radius 2.
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "EffectRadiusBeacon");

    CHECK(world.ctx->ResolveTileYield(world.At(6, 4)).energy == 1); // distance 2
    CHECK(world.ctx->ResolveTileYield(world.At(7, 4)).energy == 0); // distance 3
}

TEST_CASE("Per-effect radius: improvement-level radius is the default, per-effect values override",
          "[effects][tile][aura][radius]")
{
    // MixedRadius: improvement radius 2; energy effect inherits it, minerals effect says 1.
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "MixedRadius");

    const TileResources_t atOne = world.ctx->ResolveTileYield(world.At(5, 4));
    CHECK(atOne.energy == 1);
    CHECK(atOne.minerals == 1);

    const TileResources_t atTwo = world.ctx->ResolveTileYield(world.At(6, 4));
    CHECK(atTwo.energy == 1);
    CHECK(atTwo.minerals == 0);

    // Parse-time default check: Sensor's effect inherited the improvement's radius 2.
    const ImprovementConfig_t* pSensor = world.improvements.Find("Sensor");
    REQUIRE(pSensor != nullptr);
    REQUIRE(pSensor->effects.size() == 1);
    CHECK(pSensor->effects[0].radius == 2);
}

TEST_CASE("Aura collection: non-ThisTile and Instantaneous effects do not leak into neighbors",
          "[effects][tile][aura]")
{
    // The neighbor-aura path applies the same filter as a tile's own features: only
    // continuous ThisTile-scoped effects. WeirdAura (radius 1) carries a ThisBase +5
    // nutrients and an Instantaneous +7 minerals alongside its legitimate ThisTile +1 energy;
    // only the energy may reach the neighbor.
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "WeirdAura");

    const TileResources_t neighborYield = world.ctx->ResolveTileYield(world.At(5, 4));
    CHECK(neighborYield.energy == 1);    // the legitimate ThisTile aura effect
    CHECK(neighborYield.nutrients == 0); // ThisBase-scoped effect must not apply here
    CHECK(neighborYield.minerals == 0);  // Instantaneous effect must not apply continuously
}
