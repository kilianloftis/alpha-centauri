// Tests for TileEffectsContext: area (aura) collection, tile yield resolution, defense
// multipliers, moisture recomputation, and improvement add/remove with effects.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/TerraformSpread.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <random>

using namespace ac;
using Catch::Approx;

TEST_CASE("CollectAreaEffects: Sensor reaches Chebyshev radius for its territory owner",
          "[effects][tile][aura][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    // Base off the Sensor tile so Base's +100% defense does not stack into these checks.
    fixture.MakeFactionBase(owner, 1, 1);
    fixture.ctx->AddImprovementWithEffects(fixture.At(4, 4), "Sensor");
    const FactionId_t id = owner.GetFactionId();

    SECTION("distance 1 and 2 are covered, including diagonals")
    {
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(5, 4), id) == Approx(1.25));
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 4), id) == Approx(1.25));
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(5, 5), id) == Approx(1.25));
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 6), id) == Approx(1.25));
    }

    SECTION("distance 3 is not covered")
    {
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(7, 4), id) == Approx(1.0));
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(7, 7), id) == Approx(1.0));
    }

    SECTION("the sensor's own tile is covered")
    {
        CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(4, 4), id) == Approx(1.25));
    }
}

TEST_CASE("Sensor aura only benefits the faction that owns its territory",
          "[effects][tile][aura][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 1, 1);
    fixture.ctx->AddImprovementWithEffects(fixture.At(4, 4), "Sensor");

    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 4), owner.GetFactionId()) == Approx(1.25));
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(6, 4), other.GetFactionId()) == Approx(1.0));
}

TEST_CASE("A unit-projected defense aura only benefits the projecting faction",
          "[effects][tile][aura][unit]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& foreign = fixture.MakeFaction();

    // Unit auras are attributed to the unit's faction, not to territory — no base needed.
    fixture.MakeUnit(owner, 4, 4, {"sensor_pod"});
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(5, 4), owner.GetFactionId())
          == Approx(1.25));
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(5, 4), foreign.GetFactionId())
          == Approx(1.0));
}

TEST_CASE("Sensor on unowned territory benefits nobody", "[effects][tile][aura][territory]")
{
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "Sensor");
    // No bases -> territory unowned -> ownerFaction is k_NoFactionOwner.
    CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(4, 4), /*forFaction*/ 1) == Approx(1.0));
    CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(6, 4), /*forFaction*/ 1) == Approx(1.0));
}

TEST_CASE("CollectAreaEffects: radius-0 improvements never reach neighbors", "[effects][tile][aura]")
{
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "Bunker");

    CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(4, 4), /*forFaction*/ 1) == Approx(1.5));
    CHECK(world.ctx->ResolveTileDefenseMultiplier(world.At(5, 4), /*forFaction*/ 1) == Approx(1.0));
}

TEST_CASE("ResolveTileDefenseMultiplier: terrain and improvement bonuses combine arithmetically",
          "[effects][tile][defense][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 1, 1); // owns Sensor territory without stacking Base on (4,4)
    Tile& tile = fixture.At(4, 4);
    const FactionId_t id = owner.GetFactionId();

    // Base multiplier for a featureless tile is 1.0.
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(tile, id) == Approx(1.0));

    tile.SetRockiness(Rockiness_t::Rocky); // +25%
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(tile, id) == Approx(1.25));

    fixture.ctx->AddImprovementWithEffects(tile, "Bunker"); // +50%
    // Arithmetic combination: 1 + 0.25 + 0.50, not 1.25 * 1.5.
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(tile, id) == Approx(1.75));

    fixture.ctx->AddImprovementWithEffects(fixture.At(5, 4), "Sensor"); // +25% aura
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(tile, id) == Approx(2.0));
}

TEST_CASE("ResolveTileYield: bare tiles start at zero; SolarCollector applies elevation via amount_source",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);

    SECTION("featureless tile at sea level yields nothing")
    {
        const TileResources_t yield = world.ctx->ResolveTileYield(tile).effective;
        CHECK(yield.nutrients == 0);
        CHECK(yield.minerals == 0);
        CHECK(yield.energy == 0);
    }

    SECTION("elevation energy comes from SolarCollector amount_source")
    {
        tile.SetElevation(1000);
        CHECK(world.ctx->ResolveTileYield(tile).effective.energy == 0);
        world.ctx->AddImprovementWithEffects(tile, "SolarCollector");
        // flat +1 + ElevationEnergySeed*1 (=1)
        CHECK(world.ctx->ResolveTileYield(tile).effective.energy == 2);
        tile.SetElevation(2500);
        CHECK(world.ctx->ResolveTileYield(tile).effective.energy == 3);
    }

    SECTION("negative elevation yields only Solar flat bonus")
    {
        tile.SetElevation(-2000);
        world.ctx->AddImprovementWithEffects(tile, "SolarCollector");
        CHECK(world.ctx->ResolveTileYield(tile).effective.energy == 1);
    }
}

TEST_CASE("ResolveTileYield: each resource resolves from the matching StatId_t (field-order sanity)",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);

    tile.SetElevation(1000);   // ElevationEnergySeed 1 with SolarCollector
    tile.SetHasRiver(true);    // +1 energy
    world.ctx->AddImprovementWithEffects(tile, "Farm"); // +1 nutrients
    world.ctx->AddImprovementWithEffects(tile, "Mine"); // +2 minerals
    world.ctx->AddImprovementWithEffects(tile, "SolarCollector"); // +1 flat + seed

    const TileResources_t yield = world.ctx->ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 1);
    CHECK(yield.minerals == 2);
    CHECK(yield.energy == 3); // solar flat 1 + seed 1 + river 1
}

TEST_CASE("ResolveTileYield: terrain classification contributes through the same registry ids",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    tile.SetMoisture(Moisture_t::Wet);       // +2 nutrients
    tile.SetRockiness(Rockiness_t::Rolling); // +1 mineral

    const TileResources_t yield = world.ctx->ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 2);
    CHECK(yield.minerals == 1);
}

TEST_CASE("ResolveTileYield: a Mirror's energy aura reaches nearby tiles", "[effects][tile][yield][aura]")
{
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "Mirror");

    CHECK(world.ctx->ResolveTileYield(world.At(6, 4)).effective.energy == 1); // distance 2
    CHECK(world.ctx->ResolveTileYield(world.At(7, 4)).effective.energy == 0); // distance 3

    // Two mirrors in range stack.
    world.ctx->AddImprovementWithEffects(world.At(6, 5), "Mirror");
    CHECK(world.ctx->ResolveTileYield(world.At(6, 4)).effective.energy == 2);
}

TEST_CASE("ResolveTileYield with base effects: selector-carrying modifiers apply per matching tile",
          "[effects][tile][yield][selector]")
{
    actest::BaseFixture fixture;
    // Subject for BaseEffects_t only — keep yield tiles free of a Base improvement.
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& farmTile = fixture.At(4, 4);
    Tile& plainTile = fixture.At(5, 4);
    fixture.ctx->AddImprovementWithEffects(farmTile, "Farm");

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Farm")), "farm_booster"),
    }};

    // Farm tile: 1 (Farm) + 1 (booster). Plain tile: unaffected.
    CHECK(fixture.ctx->ResolveTileYield(farmTile, false, baseEffects).effective.nutrients == 2);
    CHECK(fixture.ctx->ResolveTileYield(plainTile, false, baseEffects).effective.nutrients == 0);
}

TEST_CASE("ResolveTileYield with base effects: BaseTile selector applies only to the base center tile",
          "[effects][tile][yield][selector]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& tile = fixture.At(4, 4);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Energy, 2.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::BaseTileSelector()), "center_booster"),
    }};

    CHECK(fixture.ctx->ResolveTileYield(tile, true, baseEffects).effective.energy == 2);
    CHECK(fixture.ctx->ResolveTileYield(tile, false, baseEffects).effective.energy == 0);
}

TEST_CASE("ResolveTileYield with base effects: AnyTile selector applies to every worked tile",
          "[effects][tile][yield][selector]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& center = fixture.At(4, 4);
    Tile& outer = fixture.At(5, 4);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Energy, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::AnyTileSelector()), "economy_sq"),
    }};

    CHECK(fixture.ctx->ResolveTileYield(center, true, baseEffects).effective.energy == 1);
    CHECK(fixture.ctx->ResolveTileYield(outer, false, baseEffects).effective.energy == 1);
}

TEST_CASE("ResolveTileYield with base effects: flat (non-selector) modifiers are NOT applied per tile",
          "[effects][tile][yield][selector]")
{
    // Flat base bonuses resolve once at the base level (FilterBaseLevelByStatId); applying them
    // per worked tile would multiply them by the number of workers.
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& tile = fixture.At(4, 4);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 2.0, ModifierOp_t::Add, EffectScope_t::ThisBase),
                       "flat_nutrient"),
    }};

    CHECK(fixture.ctx->ResolveTileYield(tile, true, baseEffects).effective.nutrients == 0);
    CHECK(fixture.ctx->ResolveTileYield(tile, false, baseEffects).effective.nutrients == 0);
}

TEST_CASE("ResolveTileYield: percentage modifiers scale a tile's own yield", "[effects][tile][yield]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& tile = fixture.At(4, 4);
    // Base moisture must be set too: AddImprovementWithEffects triggers RecomputeMoisture,
    // which re-derives the effective value from the base value (world-gen sets both).
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet); // +2 nutrients
    fixture.ctx->AddImprovementWithEffects(tile, "Farm"); // +1 nutrients

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 50.0, ModifierOp_t::AddPercent, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Farm")), "gene_splicer"),
    }};

    // (2 + 1) * 1.5 = 4.5 → FinalizeResolvedStat → 5. Per-tile yield shares the one
    // float→int rule with base-level resolve, so the worked total it feeds into
    // ResourceManager::CalculateResource_ is not truncated first and rounded again after.
    CHECK(fixture.ctx->ResolveTileYield(tile, false, baseEffects).effective.nutrients == 5);
}

TEST_CASE("RecomputeMoisture: Condenser aura raises effective moisture, derived fresh from base moisture",
          "[effects][tile][moisture]")
{
    actest::WorldFixture world;
    Tile& dryTile = world.At(4, 4);
    dryTile.SetBaseMoisture(Moisture_t::Arid);
    dryTile.SetMoisture(Moisture_t::Arid);

    // Condenser (radius 1) next door.
    world.ctx->AddImprovementWithEffects(world.At(5, 4), "Condenser");

    CHECK(dryTile.GetMoisture() == Moisture_t::Moist);     // effective value shifted
    CHECK(dryTile.GetBaseMoisture() == Moisture_t::Arid);  // terrain truth untouched

    SECTION("recompute is idempotent")
    {
        world.ctx->RecomputeMoisture(dryTile);
        world.ctx->RecomputeMoisture(dryTile);
        CHECK(dryTile.GetMoisture() == Moisture_t::Moist);
    }

    SECTION("overlapping condensers stack and clamp at Wet")
    {
        world.ctx->AddImprovementWithEffects(world.At(3, 4), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture_t::Wet); // Arid + 2

        world.ctx->AddImprovementWithEffects(world.At(4, 5), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture_t::Wet); // clamped, no overflow
    }

    SECTION("removal reverts cleanly")
    {
        world.ctx->RemoveImprovementWithEffects(world.At(5, 4), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture_t::Arid);
    }

    SECTION("removal with another condenser still in range keeps the remaining bonus")
    {
        world.ctx->AddImprovementWithEffects(world.At(3, 4), "Condenser");
        world.ctx->RemoveImprovementWithEffects(world.At(5, 4), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture_t::Moist);
    }
}

TEST_CASE("RecomputeMoisture: the moisture shift feeds back into tile yield", "[effects][tile][moisture]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    tile.SetBaseMoisture(Moisture_t::Arid);
    tile.SetMoisture(Moisture_t::Arid);
    CHECK(world.ctx->ResolveTileYield(tile).effective.nutrients == 0);

    world.ctx->AddImprovementWithEffects(tile, "Condenser");
    // Now effectively Moist: +1 nutrients through the Moist terrain feature.
    CHECK(world.ctx->ResolveTileYield(tile).effective.nutrients == 1);
}

TEST_CASE("AddImprovementWithEffects: unknown improvement ids throw",
          "[effects][tile]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    CHECK_THROWS_AS(world.ctx->AddImprovementWithEffects(tile, "OrbitalLaser"), std::runtime_error);
    CHECK(tile.GetImprovements().empty());
    world.ctx->RemoveImprovementWithEffects(tile, "OrbitalLaser"); // safe no-op for absent id
}

TEST_CASE("Aura effects at the map edge are collected without crashing", "[effects][tile][aura][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 0, 0);
    // Sensor beside the base so Base's +100% does not stack into the Sensor checks.
    fixture.ctx->AddImprovementWithEffects(fixture.At(2, 0), "Sensor");
    const FactionId_t id = owner.GetFactionId();
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(2, 0), id) == Approx(1.25));
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(1, 0), id) == Approx(1.25));
    CHECK(fixture.ctx->ResolveTileYield(fixture.At(0, 1)).effective.nutrients == 0);
}

TEST_CASE("CanBuildImprovement: excludes-list features block construction", "[effects][tile]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    const ImprovementConfig_t* pFarm = world.improvements.Find("Farm");
    REQUIRE(pFarm != nullptr);

    CHECK(CanBuildImprovement(tile, *pFarm));
    tile.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(CanBuildImprovement(tile, *pFarm)); // Farm excludes Rocky
    tile.SetRockiness(Rockiness_t::Rolling);
    CHECK(CanBuildImprovement(tile, *pFarm));
}

TEST_CASE("ResolveTileYield: sea suppresses landform/resource yields; OceanShelf is +1 nutrient",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    // Rockiness/moisture remain bound on sea tiles; Water must suppress their yield.
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    tile.SetHasRiver(true);
    world.ctx->AddImprovementWithEffects(tile, "Nutrients");

    tile.SetElevation(-100); // OceanShelf
    CHECK(tile.HasFeature("OceanShelf"));
    {
        const TileResources_t yield = world.ctx->ResolveTileYield(tile).effective;
        CHECK(yield.nutrients == 1);
        CHECK(yield.minerals == 0);
        CHECK(yield.energy == 0);
    }

    tile.SetElevation(k_OceanShelfMinElevation - 1); // Ocean
    CHECK(tile.HasFeature("Ocean"));
    {
        const TileResources_t yield = world.ctx->ResolveTileYield(tile).effective;
        CHECK(yield.nutrients == 0);
        CHECK(yield.minerals == 0);
        CHECK(yield.energy == 0);
    }
}

TEST_CASE("Terrain features: Water stacks with its depth band, general before specific",
          "[effects][tile][terrain]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);

    // Index of a feature id within GetTerrainFeatures(), or -1 when absent.
    auto indexOf = [&](std::string_view id)
    {
        const std::vector<const ImprovementConfig_t*>& rFeatures = tile.GetTerrainFeatures();
        for (std::size_t i = 0; i < rFeatures.size(); ++i)
        {
            if (rFeatures[i]->id == id)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    tile.SetElevation(-100); // shallow water
    CHECK(tile.HasFeature("Water"));
    CHECK(tile.HasFeature("OceanShelf"));
    CHECK_FALSE(tile.HasFeature("Ocean"));
    // Both present, and Water precedes the depth band it generalizes.
    REQUIRE(indexOf("Water") >= 0);
    REQUIRE(indexOf("OceanShelf") >= 0);
    CHECK(indexOf("Water") < indexOf("OceanShelf"));
    CHECK(indexOf("Ocean") == -1);

    tile.SetElevation(k_OceanShelfMinElevation - 1); // deep water
    CHECK(tile.HasFeature("Water"));
    CHECK(tile.HasFeature("Ocean"));
    CHECK_FALSE(tile.HasFeature("OceanShelf"));
    REQUIRE(indexOf("Water") >= 0);
    REQUIRE(indexOf("Ocean") >= 0);
    CHECK(indexOf("Water") < indexOf("Ocean"));
    CHECK(indexOf("OceanShelf") == -1);

    tile.SetElevation(100); // land carries neither
    CHECK_FALSE(tile.HasFeature("Water"));
    CHECK_FALSE(tile.HasFeature("Ocean"));
    CHECK_FALSE(tile.HasFeature("OceanShelf"));
    CHECK(indexOf("Water") == -1);
}

TEST_CASE("CanBuildImprovement: sea terraform excludes Ocean but allows OceanShelf",
          "[effects][tile]")
{
    actest::WorldFixture world;
    const ImprovementConfig_t* pKelp = world.improvements.Find("KelpFarm");
    REQUIRE(pKelp != nullptr);

    Tile& shelf = world.At(4, 4);
    shelf.SetElevation(-100);
    CHECK(CanBuildImprovement(shelf, *pKelp));

    Tile& ocean = world.At(5, 4);
    ocean.SetElevation(k_OceanShelfMinElevation - 1);
    CHECK_FALSE(CanBuildImprovement(ocean, *pKelp));
}

TEST_CASE("Per-effect radius: an effect's own radius grants reach beyond the host tile",
          "[effects][tile][aura][radius]")
{
    // EffectRadiusBeacon: energy effect declares radius 2.
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "EffectRadiusBeacon");

    CHECK(world.ctx->ResolveTileYield(world.At(6, 4)).effective.energy == 1); // distance 2
    CHECK(world.ctx->ResolveTileYield(world.At(7, 4)).effective.energy == 0); // distance 3
}

TEST_CASE("Per-effect radius: sibling effects may declare different radii",
          "[effects][tile][aura][radius]")
{
    // MixedRadius: energy reaches 2; minerals reaches 1.
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "MixedRadius");

    const TileResources_t atOne = world.ctx->ResolveTileYield(world.At(5, 4)).effective;
    CHECK(atOne.energy == 1);
    CHECK(atOne.minerals == 1);

    const TileResources_t atTwo = world.ctx->ResolveTileYield(world.At(6, 4)).effective;
    CHECK(atTwo.energy == 1);
    CHECK(atTwo.minerals == 0);

    // Sensor declares radius on aura effects; vision uses amount; ownership is on the improvement.
    const ImprovementConfig_t* pSensor = world.improvements.Find("Sensor");
    REQUIRE(pSensor != nullptr);
    REQUIRE(pSensor->effects.size() == 3);
    CHECK(pSensor->effects[0].radius == 2); // defense aura
    CHECK(pSensor->effects[1].radius == 0); // vision uses amount, not aura radius
    CHECK(pSensor->effects[2].radius == 2); // terrain Detect
    CHECK(pSensor->ownedByTerritory);
}

TEST_CASE("Aura collection: non-ThisTile and Instantaneous effects do not leak into neighbors",
          "[effects][tile][aura]")
{
    // The neighbor-aura path applies the same filter as a tile's own features: only
    // continuous ThisTile-scoped effects. WeirdAura (radius 1) carries a FactionGlobal +5
    // nutrients and an Instantaneous +7 minerals alongside its legitimate ThisTile +1 energy;
    // only the energy may reach the neighbor.
    actest::WorldFixture world;
    world.ctx->AddImprovementWithEffects(world.At(4, 4), "WeirdAura");

    const TileResources_t neighborYield = world.ctx->ResolveTileYield(world.At(5, 4)).effective;
    CHECK(neighborYield.energy == 1);    // the legitimate ThisTile aura effect
    CHECK(neighborYield.nutrients == 0); // FactionGlobal-scoped effect must not apply here
    CHECK(neighborYield.minerals == 0);  // Instantaneous effect must not apply continuously
}

TEST_CASE("Sensor aura wraps horizontally across the map seam",
          "[effects][tile][aura][wrap][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();
    fixture.MakeFactionBase(owner, 4, 4);
    fixture.ctx->AddImprovementWithEffects(fixture.At(0, 4), "Sensor");
    const FactionId_t id = owner.GetFactionId();

    REQUIRE(fixture.map.GetTerritory().GetOwner(0, 4) == id);
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(width - 1, 4), id) == Approx(1.25));
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(width - 2, 4), id) == Approx(1.25));
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(width - 3, 4), id) == Approx(1.0));
}

TEST_CASE("Condenser moisture aura wraps horizontally across the map seam",
          "[effects][tile][moisture][wrap]")
{
    actest::WorldFixture world;
    const int width = world.map.GetWidth();
    Tile& dryAcrossSeam = world.At(width - 1, 4);
    dryAcrossSeam.SetBaseMoisture(Moisture_t::Arid);
    dryAcrossSeam.SetMoisture(Moisture_t::Arid);

    Tile& dryTooFar = world.At(width - 2, 4);
    dryTooFar.SetBaseMoisture(Moisture_t::Arid);
    dryTooFar.SetMoisture(Moisture_t::Arid);

    // Condenser radius 1 at the west edge reaches one tile across the wrap.
    world.ctx->AddImprovementWithEffects(world.At(0, 4), "Condenser");

    CHECK(dryAcrossSeam.GetMoisture() == Moisture_t::Moist);
    CHECK(dryAcrossSeam.GetBaseMoisture() == Moisture_t::Arid);
    CHECK(dryTooFar.GetMoisture() == Moisture_t::Arid);
}

TEST_CASE("Forest suppresses rockiness/moisture but keeps resource bonuses",
          "[effects][tile][yield][forest]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    world.ctx->AddImprovementWithEffects(tile, "Forest");
    world.ctx->AddImprovementWithEffects(tile, "Nutrients");

    const TileResources_t yield = world.ctx->ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 3);
    CHECK(yield.minerals == 2);
    CHECK(yield.energy == 1);
}

TEST_CASE("Fungus overrides tile yield to 1 nutrient",
          "[effects][tile][yield][fungus]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    tile.SetHasRiver(true);
    world.ctx->AddImprovementWithEffects(tile, "Farm");
    world.ctx->AddImprovementWithEffects(tile, "Nutrients");
    tile.SetHasFungus(true);

    const TileResources_t yield = world.ctx->ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 1);
    CHECK(yield.minerals == 0);
    CHECK(yield.energy == 0);
}

TEST_CASE("Fungus yield can be boosted by base-effect selectors",
          "[effects][tile][yield][fungus][selector]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& tile = fixture.At(4, 4);
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    tile.SetHasFungus(true);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "fungus_booster"),
    }};

    const TileResources_t yield = fixture.ctx->ResolveTileYield(tile, false, baseEffects).effective;
    CHECK(yield.nutrients == 2);
    CHECK(yield.minerals == 0);
    CHECK(yield.energy == 0);
}

TEST_CASE("Monolith replaces tile yield with 2-2-2",
          "[effects][tile][yield][monolith]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(4, 4);
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    tile.SetHasRiver(true);
    world.ctx->AddImprovementWithEffects(tile, "Farm");
    world.ctx->AddImprovementWithEffects(tile, "Nutrients");
    world.ctx->AddImprovementWithEffects(tile, "Monolith");

    const TileResources_t yield = world.ctx->ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 2);
    CHECK(yield.minerals == 2);
    CHECK(yield.energy == 2);
}

TEST_CASE("TerraformSpreadGrowthAttempts matches SMAC alien_fauna formula",
          "[terraform][spread]")
{
    CHECK(TerraformSpreadGrowthAttempts(3200, 0) == 100);   // 3200 / 32
    CHECK(TerraformSpreadGrowthAttempts(3200, 100) == 56);  // 3200 / (25 + 32)
    CHECK(TerraformSpreadGrowthAttempts(0, 0) == 0);
}

TEST_CASE("TrySpreadTerraformFromTile spreads Forest, prefers arid, clears fungus",
          "[terraform][spread]")
{
    actest::WorldFixture world;
    Tile& origin = world.At(4, 4);
    origin.SetElevation(100);
    origin.SetRockiness(Rockiness_t::Flat);
    world.ctx->AddImprovementWithEffects(origin, "Forest");

    // Only two land candidates: arid+fungus should beat wet.
    ForEachTileInChebyshevRadius(origin, world.map, 1, false,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            pNeighbor->SetElevation(-1000); // sea: ineligible for Forest
        });

    Tile& arid = world.At(5, 4);
    arid.SetElevation(100);
    arid.SetRockiness(Rockiness_t::Flat);
    arid.SetMoisture(Moisture_t::Arid);
    arid.SetHasFungus(true);

    Tile& wet = world.At(3, 4);
    wet.SetElevation(100);
    wet.SetRockiness(Rockiness_t::Flat);
    wet.SetMoisture(Moisture_t::Wet);

    REQUIRE(TrySpreadTerraformFromTile(origin, world.map, *world.ctx));
    CHECK(arid.HasImprovement("Forest"));
    CHECK_FALSE(arid.GetHasFungus());
    CHECK_FALSE(wet.HasImprovement("Forest"));
}

TEST_CASE("TrySpreadTerraformFromTile does not spread Forest onto Rocky tiles",
          "[terraform][spread]")
{
    actest::WorldFixture world;
    Tile& origin = world.At(4, 4);
    origin.SetElevation(100);
    world.ctx->AddImprovementWithEffects(origin, "Forest");

    ForEachTileInChebyshevRadius(origin, world.map, 1, false,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            pNeighbor->SetElevation(100);
            pNeighbor->SetRockiness(Rockiness_t::Rocky);
            pNeighbor->SetMoisture(Moisture_t::Arid);
        });

    CHECK_FALSE(TrySpreadTerraformFromTile(origin, world.map, *world.ctx));
    ForEachTileInChebyshevRadius(origin, world.map, 1, false,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            CHECK_FALSE(pNeighbor->HasImprovement("Forest"));
        });
}

TEST_CASE("SpreadTerraformImprovements eventually spreads from a sampled Forest",
          "[terraform][spread]")
{
    actest::WorldFixture world;
    Tile& origin = world.At(4, 4);
    origin.SetElevation(100);
    origin.SetRockiness(Rockiness_t::Flat);
    world.ctx->AddImprovementWithEffects(origin, "Forest");

    Tile& neighbor = world.At(5, 4);
    neighbor.SetElevation(100);
    neighbor.SetRockiness(Rockiness_t::Flat);
    neighbor.SetMoisture(Moisture_t::Arid);

    // Leave other adjacent tiles rocky so only `neighbor` can receive the spread.
    ForEachTileInChebyshevRadius(origin, world.map, 1, false,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            if (pNeighbor != &neighbor)
            {
                pNeighbor->SetRockiness(Rockiness_t::Rocky);
            }
        });

    std::mt19937 rng(1);
    bool spread = false;
    for (int i = 0; i < 2000 && !spread; ++i)
    {
        SpreadTerraformImprovements(world.map, *world.ctx, /*turnIndex=*/0, rng);
        spread = neighbor.HasImprovement("Forest");
    }
    CHECK(spread);
}
