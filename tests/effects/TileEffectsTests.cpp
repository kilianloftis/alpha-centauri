// Tests for TileEffectsContext: area (aura) collection, tile yield resolution, tile defense
// bonuses on a defender, moisture recomputation, and improvement add/remove with effects.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/map/OccupantCoexistence.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/terraform/TerraformSpread.h"
#include "game/terraform/TerraformRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <random>

using namespace ac;
using Catch::Approx;

namespace
{

const std::vector<std::string> k_ArmoredLand{"test_chassis", "test_weak_armor"};
const std::vector<std::string> k_LandGun{"test_chassis", "test_weapon"};

} // namespace

TEST_CASE("Sensor reaches defenders within Chebyshev radius 2 for its territory owner",
          "[effects][tile][aura][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    // Base off the Sensor tile so Base's defense bonus does not stack into these checks.
    fixture.MakeFactionBase(owner, 8, 2);
    fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Sensor");
    Unit& defender = fixture.MakeUnit(owner, 8, 8, k_ArmoredLand);
    const Unit& attacker = fixture.MakeUnit(enemy, 0, 0, k_LandGun);

    SECTION("distance 1 and 2 are covered, including diagonals")
    {
        CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 9, 9) == Approx(1.25));
        CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 10, 10) == Approx(1.25));
        CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 8, 10) == Approx(1.25));
        CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 8, 12) == Approx(1.25));
    }

    SECTION("distance 3 is not covered")
    {
        CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 11, 11) == Approx(1.0));
        CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 8, 14) == Approx(1.0));
    }

    SECTION("the sensor's own tile is covered")
    {
        CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 8, 8) == Approx(1.25));
    }
}

TEST_CASE("Sensor in owned territory only benefits the territory owner",
          "[effects][tile][aura][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 8, 2);
    fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Sensor");
    const Unit& ownerDefender = fixture.MakeUnit(owner, 10, 10, k_ArmoredLand);
    const Unit& otherDefender = fixture.MakeUnit(other, 10, 10, k_ArmoredLand);

    CHECK(actest::ConventionalDefenseRating(ownerDefender, otherDefender) == Approx(1.25));
    CHECK(actest::ConventionalDefenseRating(otherDefender, ownerDefender) == Approx(1.0));
}

TEST_CASE("Sensor in unowned territory benefits any real faction's defender",
          "[effects][tile][aura][territory]")
{
    actest::FactionFixture fixture;
    Faction& first = fixture.MakeFaction();
    Faction& second = fixture.MakeFaction();
    fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Sensor");
    Unit& defender = fixture.MakeUnit(first, 8, 8, k_ArmoredLand);
    const Unit& attacker = fixture.MakeUnit(second, 0, 0, k_LandGun);

    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 8, 8) == Approx(1.25));
    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 10, 10) == Approx(1.25));
}

TEST_CASE("Sensor never benefits a native-life faction's defender",
          "[effects][tile][aura][territory]")
{
    actest::FactionFixture fixture;
    Faction& planet = actest::AddNativeLifeFaction(fixture);
    Faction& enemy = fixture.MakeFaction();
    fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Sensor");
    const Unit& defender = fixture.MakeUnit(planet, 8, 8, k_ArmoredLand);
    const Unit& attacker = fixture.MakeUnit(enemy, 0, 0, k_LandGun);

    CHECK(actest::ConventionalDefenseRating(defender, attacker) == Approx(1.0));
}

TEST_CASE("A unit-projected defense aura only benefits the projecting faction",
          "[effects][tile][aura][unit]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& foreign = fixture.MakeFaction();

    // Unit auras are attributed to the unit's faction, not to territory — no base needed.
    fixture.MakeUnit(owner, 8, 8, {"test_chassis", "sensor_pod"});
    const Unit& ownerDefender = fixture.MakeUnit(owner, 9, 9, k_ArmoredLand);
    const Unit& foreignDefender = fixture.MakeUnit(foreign, 9, 9, k_ArmoredLand);
    CHECK(actest::ConventionalDefenseRating(ownerDefender, foreignDefender) == Approx(1.25));
    CHECK(actest::ConventionalDefenseRating(foreignDefender, ownerDefender) == Approx(1.0));
}

TEST_CASE("CollectAreaEffects: radius-0 improvements never reach neighbors", "[effects][tile][aura]")
{
    actest::FactionFixture fixture;
    Faction& defending = fixture.MakeFaction();
    Faction& attacking = fixture.MakeFaction();
    fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Bunker");
    Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
    const Unit& attacker = fixture.MakeUnit(attacking, 0, 0, k_LandGun);

    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 8, 8) == Approx(1.5));
    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 9, 9) == Approx(1.0));
}

TEST_CASE("Tile defense bonuses each multiply the defender's rating",
          "[effects][tile][defense][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 8, 2); // owns Sensor territory without stacking Base on (8,8)
    Tile& tile = fixture.At(8, 8);
    const Unit& defender = fixture.MakeUnit(owner, 8, 8, k_ArmoredLand);
    const Unit& attacker = fixture.MakeUnit(enemy, 0, 0, k_LandGun);

    CHECK(actest::ConventionalDefenseRating(defender, attacker) == Approx(1.0));

    tile.SetRockiness(Rockiness_t::Rocky);
    CHECK(actest::ConventionalDefenseRating(defender, attacker) == Approx(1.5));

    fixture.ctx.AddOccupantWithEffects(tile, "Bunker");
    CHECK(actest::ConventionalDefenseRating(defender, attacker) == Approx(2.25));

    fixture.ctx.AddOccupantWithEffects(fixture.At(9, 9), "Sensor");
    CHECK(actest::ConventionalDefenseRating(defender, attacker) == Approx(2.8125));
}

TEST_CASE("Rocky defends land and sea units off a base, unless fungus meets a native attacker",
          "[effects][tile][defense]")
{
    actest::FactionFixture fixture;
    Faction& defending = fixture.MakeFaction();
    Faction& attacking = fixture.MakeFaction();
    Tile& tile = fixture.At(8, 8);
    tile.SetRockiness(Rockiness_t::Rocky);
    const Unit& gun = fixture.MakeUnit(attacking, 0, 0, k_LandGun);
    const Unit& worm = fixture.MakeUnit(attacking, 0, 2, {"native_life_chassis", "test_weapon"});

    SECTION("a land defender gets x1.5")
    {
        const Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
        CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(1.5));
    }

    SECTION("an air defender gets nothing")
    {
        const Unit& flyer =
            fixture.MakeUnit(defending, 8, 8, {"test_flight_chassis", "test_weak_armor"});
        CHECK(actest::ConventionalDefenseRating(flyer, gun) == Approx(1.0));
    }

    SECTION("rocky fungus still gives x1.5 against a conventional attacker, not x2.25")
    {
        fixture.ctx.AddOccupantWithEffects(tile, "Fungus");
        const Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
        CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(1.5));
    }

    SECTION("rocky fungus gives nothing against a native attacker")
    {
        fixture.ctx.AddOccupantWithEffects(tile, "Fungus");
        const Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
        CHECK(actest::ConventionalDefenseRating(defender, worm) == Approx(1.0));
    }
}

TEST_CASE("Fungus defends x1.5, or x2 for a native defender, but not against natives",
          "[effects][tile][defense]")
{
    actest::FactionFixture fixture;
    Faction& defending = fixture.MakeFaction();
    Faction& attacking = fixture.MakeFaction();
    fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Fungus");
    const Unit& gun = fixture.MakeUnit(attacking, 0, 0, k_LandGun);
    const Unit& worm = fixture.MakeUnit(attacking, 0, 2, {"native_life_chassis", "test_weapon"});

    SECTION("a conventional defender gets x1.5")
    {
        const Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
        CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(1.5));
    }

    SECTION("a native defender gets x2")
    {
        const Unit& native =
            fixture.MakeUnit(defending, 8, 8, {"native_life_chassis", "test_weak_armor"});
        CHECK(actest::ConventionalDefenseRating(native, gun) == Approx(2.0));
    }

    SECTION("a native attacker cancels it")
    {
        const Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
        CHECK(actest::ConventionalDefenseRating(defender, worm) == Approx(1.0));
    }
}

TEST_CASE("Sea fungus gives no defense bonus", "[effects][tile][defense]")
{
    actest::FactionFixture fixture;
    Faction& defending = fixture.MakeFaction();
    Faction& attacking = fixture.MakeFaction();
    Tile& sea = fixture.At(8, 8);
    sea.SetElevation(-100);
    fixture.ctx.AddOccupantWithEffects(sea, "Fungus");
    const Unit& defender =
        fixture.MakeUnit(defending, 8, 8, {"test_sea_chassis", "test_weak_armor"});
    const Unit& gun = fixture.MakeUnit(attacking, 0, 0, k_LandGun);

    CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(1.0));
}

TEST_CASE("Overlapping Sensors give one bonus in either kind of fight",
          "[effects][tile][aura][defense]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 8, 2);
    const Unit& defender = fixture.MakeUnit(owner, 9, 9, k_ArmoredLand);
    const Unit& attacker = fixture.MakeUnit(enemy, 0, 0, k_LandGun);
    const double psiBaseline = actest::PsiDefenseRating(defender, attacker);

    fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Sensor");
    fixture.ctx.AddOccupantWithEffects(fixture.At(10, 10), "Sensor");
    CHECK(actest::ConventionalDefenseRating(defender, attacker) == Approx(1.25));
    CHECK(actest::PsiDefenseRating(defender, attacker) == Approx(psiBaseline * 1.25));
}

TEST_CASE("Forest defends x1.5 against a land attacker only", "[effects][tile][defense]")
{
    actest::FactionFixture fixture;
    Faction& defending = fixture.MakeFaction();
    Faction& attacking = fixture.MakeFaction();
    Tile& tile = fixture.At(8, 8);
    const Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
    const Unit& gun = fixture.MakeUnit(attacking, 0, 0, k_LandGun);
    const Unit& flyer =
        fixture.MakeUnit(attacking, 0, 2, {"test_flight_chassis", "test_weapon"});
    const double psiBaseline = actest::PsiDefenseRating(defender, gun);

    fixture.ctx.AddOccupantWithEffects(tile, "Forest");
    CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(1.5));
    CHECK(actest::ConventionalDefenseRating(defender, flyer) == Approx(1.0));
    CHECK(actest::PsiDefenseRating(defender, gun) == Approx(psiBaseline));

    SECTION("a base on the forest gives only its own bonus")
    {
        fixture.MakeFactionBase(defending, 8, 8);
        CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(1.25));
    }
}

TEST_CASE("Terrain gives its largest bonus, not the product", "[effects][tile][defense]")
{
    actest::FactionFixture fixture;
    Faction& defending = fixture.MakeFaction();
    Faction& attacking = fixture.MakeFaction();
    Tile& tile = fixture.At(8, 8);
    tile.SetRockiness(Rockiness_t::Rocky);
    fixture.ctx.AddOccupantWithEffects(tile, "Fungus");
    const Unit& gun = fixture.MakeUnit(attacking, 0, 0, k_LandGun);

    SECTION("a native defender on rocky fungus gets Rocky's x1.5, not Fungus's x2")
    {
        const Unit& native =
            fixture.MakeUnit(defending, 8, 8, {"native_life_chassis", "test_weak_armor"});
        CHECK(actest::ConventionalDefenseRating(native, gun) == Approx(1.5));
    }

    SECTION("terrain and a Bunker multiply")
    {
        fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Bunker");
        const Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
        CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(2.25));
    }
}

TEST_CASE("A rocky sea floor gives no defense bonus", "[effects][tile][defense]")
{
    actest::FactionFixture fixture;
    Faction& defending = fixture.MakeFaction();
    Faction& attacking = fixture.MakeFaction();
    Tile& sea = fixture.At(8, 8);
    sea.SetElevation(-100);
    sea.SetRockiness(Rockiness_t::Rocky);
    const Unit& defender =
        fixture.MakeUnit(defending, 8, 8, {"test_sea_chassis", "test_weak_armor"});
    const Unit& gun = fixture.MakeUnit(attacking, 0, 0, k_LandGun);

    CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(1.0));
}

TEST_CASE("Bunker defends against land and sea attackers, not air", "[effects][tile][defense]")
{
    actest::FactionFixture fixture;
    Faction& defending = fixture.MakeFaction();
    Faction& attacking = fixture.MakeFaction();
    fixture.ctx.AddOccupantWithEffects(fixture.At(8, 8), "Bunker");
    const Unit& defender = fixture.MakeUnit(defending, 8, 8, k_ArmoredLand);
    const Unit& gun = fixture.MakeUnit(attacking, 0, 0, k_LandGun);
    const Unit& flyer = fixture.MakeUnit(attacking, 0, 2, {"test_flight_chassis", "test_weapon"});

    CHECK(actest::ConventionalDefenseRating(defender, gun) == Approx(1.5));
    CHECK(actest::ConventionalDefenseRating(defender, flyer) == Approx(1.0));
}

TEST_CASE("ResolveTileYield: bare tiles start at zero; SolarCollector applies elevation via amount_source",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);

    SECTION("featureless tile at sea level yields nothing")
    {
        const TileResources_t yield = world.ctx.ResolveTileYield(tile).effective;
        CHECK(yield.nutrients == 0);
        CHECK(yield.minerals == 0);
        CHECK(yield.energy == 0);
    }

    SECTION("a solar collector's whole yield is ceil(elevation / step)")
    {
        tile.SetElevation(1000);
        CHECK(world.ctx.ResolveTileYield(tile).effective.energy == 0);
        world.ctx.AddOccupantWithEffects(tile, "SolarCollector");
        CHECK(world.ctx.ResolveTileYield(tile).effective.energy == 1);
        tile.SetElevation(2500);
        CHECK(world.ctx.ResolveTileYield(tile).effective.energy == 3);
    }

    SECTION("rounded up, so any land above sea level is worth a point")
    {
        tile.SetElevation(1);
        world.ctx.AddOccupantWithEffects(tile, "SolarCollector");
        CHECK(world.ctx.ResolveTileYield(tile).effective.energy == 1);
    }

    SECTION("sea level and below yield nothing")
    {
        world.ctx.AddOccupantWithEffects(tile, "SolarCollector");
        tile.SetElevation(0);
        CHECK(world.ctx.ResolveTileYield(tile).effective.energy == 0);
        tile.SetElevation(-2000);
        CHECK(world.ctx.ResolveTileYield(tile).effective.energy == 0);
    }
}

TEST_CASE("ResolveTileYield: each resource resolves from the matching StatId_t (field-order sanity)",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);

    tile.SetElevation(1000); // one elevation band
    // River excludes Mine, so a river here would remove the mine.
    world.ctx.AddOccupantWithEffects(tile, "Farm"); // +1 nutrients
    world.ctx.AddOccupantWithEffects(tile, "Mine"); // +2 minerals (fixture Mine)
    // SolarCollector excludes Mine, so adding it would remove the mine.
    world.ctx.AddOccupantWithEffects(tile, "Mirror");

    const TileResources_t yield = world.ctx.ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 1);
    CHECK(yield.minerals == 2);
    CHECK(yield.energy == 1); // mirror ceil(1000/1000) = 1
}

TEST_CASE("ResolveTileYield: terrain classification contributes through the same registry ids",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);
    tile.SetMoisture(Moisture_t::Wet);       // +2 nutrients
    tile.SetRockiness(Rockiness_t::Rolling); // +1 mineral

    const TileResources_t yield = world.ctx.ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 2);
    CHECK(yield.minerals == 1);
}

TEST_CASE("ResolveTileYield: a Mirror boosts adjacent solar collectors only",
          "[effects][tile][yield][aura]")
{
    actest::WorldFixture world;
    world.ctx.AddOccupantWithEffects(world.At(8, 8), "Mirror");

    SECTION("a bare neighbour gets nothing — the aura is not a blanket +1")
    {
        CHECK(world.ctx.ResolveTileYield(world.At(9, 9)).effective.energy == 0);
    }

    SECTION("an adjacent solar collector gets +1 on top of its own elevation")
    {
        Tile& neighbour = world.At(9, 9);
        neighbour.SetElevation(2000);
        world.ctx.AddOccupantWithEffects(neighbour, "SolarCollector");
        CHECK(world.ctx.ResolveTileYield(neighbour).effective.energy == 3); // 2 bands + mirror 1
    }

    SECTION("out of range gets nothing")
    {
        Tile& far = world.At(10, 10); // distance 2
        far.SetElevation(1000);
        world.ctx.AddOccupantWithEffects(far, "SolarCollector");
        CHECK(world.ctx.ResolveTileYield(far).effective.energy == 1); // its own band only
    }
}

TEST_CASE("ResolveTileYield: a Mirror is its own solar collector but does not buff Mirrors",
          "[effects][tile][yield][aura]")
{
    actest::WorldFixture world;
    Tile& host = world.At(8, 8);
    host.SetElevation(2000);
    world.ctx.AddOccupantWithEffects(host, "Mirror");

    // Counts as a collector: two elevation bands. Does not add its own +1 (min_radius 1).
    CHECK(world.ctx.ResolveTileYield(host).effective.energy == 2);

    // Neighbouring Mirrors do not buff each other — only SolarCollector tiles do.
    Tile& second = world.At(9, 9);
    second.SetElevation(1000);
    world.ctx.AddOccupantWithEffects(second, "Mirror");
    CHECK(world.ctx.ResolveTileYield(host).effective.energy == 2);
    CHECK(world.ctx.ResolveTileYield(second).effective.energy == 1);
}

TEST_CASE("ResolveTileYield with base effects: selector-carrying modifiers apply per matching tile",
          "[effects][tile][yield][selector]")
{
    actest::BaseFixture fixture;
    // Subject for BaseEffects_t only — keep yield tiles free of a Base improvement.
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& farmTile = fixture.At(8, 8);
    Tile& plainTile = fixture.At(9, 9);
    fixture.ctx.AddOccupantWithEffects(farmTile, "Farm");

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Farm")), "farm_booster"),
    }};

    // Farm tile: 1 (Farm) + 1 (booster). Plain tile: unaffected.
    CHECK(fixture.ctx.ResolveTileYield(farmTile, false, baseEffects).effective.nutrients == 2);
    CHECK(fixture.ctx.ResolveTileYield(plainTile, false, baseEffects).effective.nutrients == 0);
}

TEST_CASE("ResolveTileYield with base effects: BaseTile selector applies only to the base center tile",
          "[effects][tile][yield][selector]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Energy, 2.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::BaseTileSelector()), "center_booster"),
    }};

    CHECK(fixture.ctx.ResolveTileYield(tile, true, baseEffects).effective.energy == 2);
    CHECK(fixture.ctx.ResolveTileYield(tile, false, baseEffects).effective.energy == 0);
}

TEST_CASE("ResolveTileYield: Rocky minerals match Rolling without a mine",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& rolling = world.At(8, 8);
    rolling.SetRockiness(Rockiness_t::Rolling);
    Tile& rocky = world.At(9, 9);
    rocky.SetRockiness(Rockiness_t::Rocky);

    CHECK(world.ctx.ResolveTileYield(rolling).effective.minerals == 1);
    CHECK(world.ctx.ResolveTileYield(rocky).effective.minerals == 1);
}

TEST_CASE("ResolveTileYield: Rocky zeroes rainfall but keeps jungle nutrients",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& wetRocky = world.At(8, 8);
    wetRocky.SetRockiness(Rockiness_t::Rocky);
    wetRocky.SetMoisture(Moisture_t::Wet);

    Tile& moistRockyJungle = world.At(9, 9);
    moistRockyJungle.SetRockiness(Rockiness_t::Rocky);
    moistRockyJungle.SetMoisture(Moisture_t::Moist);
    world.ctx.AddOccupantWithEffects(moistRockyJungle, "MonsoonJungle");

    CHECK(world.ctx.ResolveTileYield(wetRocky).effective.nutrients == 0);
    // Rainfall suppressed; jungle landmark +1 remains (SMAC order).
    CHECK(world.ctx.ResolveTileYield(moistRockyJungle).effective.nutrients == 1);
}

TEST_CASE("ResolveTileYield: Mine nutrient penalty only while nutrients so far exceed 1",
          "[effects][tile][yield][mine]")
{
    actest::WorldFixture world;

    // Base moisture must be set: AddOccupantWithEffects recomputes effective moisture from it.
    Tile& arid = world.At(6, 6);
    arid.SetBaseMoisture(Moisture_t::Arid);
    arid.SetMoisture(Moisture_t::Arid);
    world.ctx.AddOccupantWithEffects(arid, "Mine");
    CHECK(world.ctx.ResolveTileYield(arid).effective.nutrients == 0);

    Tile& moist = world.At(7, 7);
    moist.SetBaseMoisture(Moisture_t::Moist);
    moist.SetMoisture(Moisture_t::Moist);
    world.ctx.AddOccupantWithEffects(moist, "Mine");
    CHECK(world.ctx.ResolveTileYield(moist).effective.nutrients == 1);

    Tile& wet = world.At(8, 8);
    wet.SetBaseMoisture(Moisture_t::Wet);
    wet.SetMoisture(Moisture_t::Wet);
    world.ctx.AddOccupantWithEffects(wet, "Mine");
    CHECK(world.ctx.ResolveTileYield(wet).effective.nutrients == 1);

    Tile& wetFarm = world.At(9, 9);
    wetFarm.SetBaseMoisture(Moisture_t::Wet);
    wetFarm.SetMoisture(Moisture_t::Wet);
    world.ctx.AddOccupantWithEffects(wetFarm, "Farm");
    world.ctx.AddOccupantWithEffects(wetFarm, "Mine");
    CHECK(world.ctx.ResolveTileYield(wetFarm).effective.nutrients == 2);

    Tile& rockyWet = world.At(10, 10);
    rockyWet.SetRockiness(Rockiness_t::Rocky);
    rockyWet.SetBaseMoisture(Moisture_t::Wet);
    rockyWet.SetMoisture(Moisture_t::Wet);
    world.ctx.AddOccupantWithEffects(rockyWet, "Mine");
    CHECK(world.ctx.ResolveTileYield(rockyWet).effective.nutrients == 0);

    Tile& rockyJungleMine = world.At(11, 11);
    rockyJungleMine.SetRockiness(Rockiness_t::Rocky);
    rockyJungleMine.SetBaseMoisture(Moisture_t::Moist);
    rockyJungleMine.SetMoisture(Moisture_t::Moist);
    world.ctx.AddOccupantWithEffects(rockyJungleMine, "MonsoonJungle");
    world.ctx.AddOccupantWithEffects(rockyJungleMine, "Mine");
    // Probe is jungle +1 only; penalty stays off.
    CHECK(world.ctx.ResolveTileYield(rockyJungleMine).effective.nutrients == 1);
}

TEST_CASE("ResolveTileYield: deep ocean nutrients match the shelf row",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& deep = world.At(8, 8);
    deep.SetElevation(deep.MapRules().oceanShelfMeters - 1);

    CHECK(deep.IsWater());
    CHECK_FALSE(deep.HasFeature("OceanShelf"));
    CHECK(deep.HasFeature("Ocean"));
    CHECK(world.ctx.ResolveTileYield(deep).effective.nutrients == 1);
}

TEST_CASE("ResolveTileYield with base effects: Forest selector boosts forest squares",
          "[effects][tile][yield][selector]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& forestTile = fixture.At(8, 8);
    Tile& plainTile = fixture.At(9, 9);
    fixture.ctx.AddOccupantWithEffects(forestTile, "Forest");

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Forest")), "tree_farm"),
        actest::Active(pool.StatMod(StatId_t::Energy, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Forest")), "hybrid_forest"),
    }};

    // Forest 1-2-1 plus Tree Farm nutrient and Hybrid energy.
    const TileResources_t forest =
        fixture.ctx.ResolveTileYield(forestTile, false, baseEffects).effective;
    CHECK(forest.nutrients == 2);
    CHECK(forest.minerals == 2);
    CHECK(forest.energy == 2);
    CHECK(fixture.ctx.ResolveTileYield(plainTile, false, baseEffects).effective.nutrients == 0);
}

TEST_CASE("ResolveTileYield with base effects: AnyTile selector applies to every worked tile",
          "[effects][tile][yield][selector]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& center = fixture.At(8, 8);
    Tile& outer = fixture.At(9, 9);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Energy, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::AnyTileSelector()), "economy_sq"),
    }};

    CHECK(fixture.ctx.ResolveTileYield(center, true, baseEffects).effective.energy == 1);
    CHECK(fixture.ctx.ResolveTileYield(outer, false, baseEffects).effective.energy == 1);
}

TEST_CASE("ResolveTileYield with base effects: flat (non-selector) modifiers are NOT applied per tile",
          "[effects][tile][yield][selector]")
{
    // Flat base bonuses resolve once at the base level (FilterBaseLevelByStatId); applying them
    // per worked tile would multiply them by the number of workers.
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 2.0, ModifierOp_t::Add, EffectScope_t::ThisBase),
                       "flat_nutrient"),
    }};

    CHECK(fixture.ctx.ResolveTileYield(tile, true, baseEffects).effective.nutrients == 0);
    CHECK(fixture.ctx.ResolveTileYield(tile, false, baseEffects).effective.nutrients == 0);
}

TEST_CASE("ResolveTileYield: percentage modifiers scale a tile's own yield", "[effects][tile][yield]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);
    // Base moisture must be set too: AddOccupantWithEffects triggers RecomputeMoisture,
    // which re-derives the effective value from the base value (world-gen sets both).
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet); // +2 nutrients
    fixture.ctx.AddOccupantWithEffects(tile, "Farm"); // +1 nutrients

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 50.0, ModifierOp_t::AddPercent, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Farm")), "gene_splicer"),
    }};

    // (2 + 1) * 1.5 = 4.5 → FinalizeResolvedStat → 5. Per-tile yield shares the one
    // float→int rule with base-level resolve, so the worked total it feeds into
    // ResourceManager::CalculateResource_ is not truncated first and rounded again after.
    CHECK(fixture.ctx.ResolveTileYield(tile, false, baseEffects).effective.nutrients == 5);
}

TEST_CASE("RecomputeMoisture: Condenser aura raises effective moisture, derived fresh from base moisture",
          "[effects][tile][moisture]")
{
    actest::WorldFixture world;
    Tile& dryTile = world.At(8, 8);
    dryTile.SetBaseMoisture(Moisture_t::Arid);
    dryTile.SetMoisture(Moisture_t::Arid);

    // Condenser (radius 1) next door.
    world.ctx.AddOccupantWithEffects(world.At(9, 9), "Condenser");

    CHECK(dryTile.GetMoisture() == Moisture_t::Moist);     // effective value shifted
    CHECK(dryTile.GetBaseMoisture() == Moisture_t::Arid);  // terrain truth untouched

    SECTION("recompute is idempotent")
    {
        world.ctx.RecomputeMoisture(dryTile);
        world.ctx.RecomputeMoisture(dryTile);
        CHECK(dryTile.GetMoisture() == Moisture_t::Moist);
    }

    SECTION("overlapping condensers stack and clamp at Wet")
    {
        world.ctx.AddOccupantWithEffects(world.At(7, 7), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture_t::Wet); // Arid + 2

        world.ctx.AddOccupantWithEffects(world.At(7, 9), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture_t::Wet); // clamped, no overflow
    }

    SECTION("removal reverts cleanly")
    {
        world.ctx.RemoveOccupantWithEffects(world.At(9, 9), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture_t::Arid);
    }

    SECTION("removal with another condenser still in range keeps the remaining bonus")
    {
        world.ctx.AddOccupantWithEffects(world.At(7, 7), "Condenser");
        world.ctx.RemoveOccupantWithEffects(world.At(9, 9), "Condenser");
        CHECK(dryTile.GetMoisture() == Moisture_t::Moist);
    }
}

TEST_CASE("RecomputeClimate: lowland forest carries coastal rain further inland",
          "[effects][tile][moisture]")
{
    // Sea at the west edge seeds each row's rain belt; on bare flats it dries out within a few
    // tiles. A forest resets the dry count for its own row from the forest to one tile past it.
    actest::WorldFixture world;
    for (const std::unique_ptr<Tile>& pTile : world.map.GetTiles())
    {
        pTile->SetElevation(pTile->GetX() <= 3 ? -100 : 200);
    }
    constexpr int y = 8;
    Tile& rInland = world.At(24, y);
    world.ctx.RecomputeClimate();
    REQUIRE(rInland.GetMoisture() == Moisture_t::Arid);

    for (int x = 8; x <= 20; x += 2)
    {
        world.ctx.AddOccupantWithEffects(world.At(x, y), "Forest");
    }
    CHECK(rInland.GetMoisture() == Moisture_t::Arid); // planting does not re-run the climate

    world.ctx.RecomputeClimate();
    CHECK(rInland.GetMoisture() == Moisture_t::Moist);
}

TEST_CASE("RecomputeMoisture: the moisture shift feeds back into tile yield", "[effects][tile][moisture]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);
    tile.SetBaseMoisture(Moisture_t::Arid);
    tile.SetMoisture(Moisture_t::Arid);
    CHECK(world.ctx.ResolveTileYield(tile).effective.nutrients == 0);

    world.ctx.AddOccupantWithEffects(tile, "Condenser");
    // Now effectively Moist: +1 nutrients through the Moist terrain feature.
    CHECK(world.ctx.ResolveTileYield(tile).effective.nutrients == 1);
}

TEST_CASE("AddOccupantWithEffects: unknown improvement ids throw",
          "[effects][tile]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);
    CHECK_THROWS_AS(world.ctx.AddOccupantWithEffects(tile, "OrbitalLaser"), std::runtime_error);
    CHECK(tile.GetImprovements().empty());
    world.ctx.RemoveOccupantWithEffects(tile, "OrbitalLaser"); // safe no-op for absent id
}

TEST_CASE("Aura effects at the map edge are collected without crashing", "[effects][tile][aura][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 8, 0);
    // Sensor beside the base so Base's defense bonus does not stack into the Sensor checks.
    fixture.ctx.AddOccupantWithEffects(fixture.At(10, 2), "Sensor");
    Unit& defender = fixture.MakeUnit(owner, 10, 2, k_ArmoredLand);
    const Unit& attacker = fixture.MakeUnit(enemy, 0, 8, k_LandGun);
    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 10, 2) == Approx(1.25));
    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, 9, 1) == Approx(1.25));
    CHECK(fixture.ctx.ResolveTileYield(fixture.At(7, 1)).effective.nutrients == 0);
}

TEST_CASE("CanBuildImprovement: excludes-list features block construction", "[effects][tile]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);
    const ImprovementConfig_t* pFarm = world.improvements.Find("Farm");
    REQUIRE(pFarm != nullptr);

    CHECK(CanBuildImprovement(tile, *pFarm, {}, {}));
    tile.SetRockiness(Rockiness_t::Rocky);
    CHECK_FALSE(CanBuildImprovement(tile, *pFarm, {}, {})); // Farm excludes Rocky
    tile.SetRockiness(Rockiness_t::Rolling);
    CHECK(CanBuildImprovement(tile, *pFarm, {}, {}));
}

TEST_CASE("ResolveTileYield: sea suppresses landform/resource yields; Ocean and shelf are +1 nutrient",
          "[effects][tile][yield]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);
    // Rockiness/moisture remain bound on sea tiles; Water must suppress their yield.
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    tile.SetHasRiver(true);
    world.ctx.AddOccupantWithEffects(tile, "Nutrients");

    tile.SetElevation(-100); // OceanShelf
    CHECK(tile.HasFeature("OceanShelf"));
    {
        const TileResources_t yield = world.ctx.ResolveTileYield(tile).effective;
        CHECK(yield.nutrients == 1);
        CHECK(yield.minerals == 0);
        CHECK(yield.energy == 0);
    }

    tile.SetElevation(tile.MapRules().oceanShelfMeters - 1); // Ocean
    CHECK(tile.HasFeature("Ocean"));
    {
        const TileResources_t yield = world.ctx.ResolveTileYield(tile).effective;
        CHECK(yield.nutrients == 1);
        CHECK(yield.minerals == 0);
        CHECK(yield.energy == 0);
    }
}

TEST_CASE("Terrain features: Water stacks with its depth band, general before specific",
          "[effects][tile][terrain]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);

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

    tile.SetElevation(tile.MapRules().oceanShelfMeters - 1); // deep water
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

    Tile& shelf = world.At(8, 8);
    shelf.SetElevation(-100);
    CHECK(CanBuildImprovement(shelf, *pKelp, {}, {}));

    Tile& ocean = world.At(9, 9);
    ocean.SetElevation(ocean.MapRules().oceanShelfMeters - 1);
    CHECK_FALSE(CanBuildImprovement(ocean, *pKelp, {}, {}));
}

TEST_CASE("Per-effect radius: an effect's own radius grants reach beyond the host tile",
          "[effects][tile][aura][radius]")
{
    // EffectRadiusBeacon: energy effect declares radius 2.
    actest::WorldFixture world;
    world.ctx.AddOccupantWithEffects(world.At(8, 8), "EffectRadiusBeacon");

    CHECK(world.ctx.ResolveTileYield(world.At(10, 10)).effective.energy == 1); // distance 2
    CHECK(world.ctx.ResolveTileYield(world.At(11, 11)).effective.energy == 0); // distance 3
}

TEST_CASE("Per-effect radius: sibling effects may declare different radii",
          "[effects][tile][aura][radius]")
{
    // MixedRadius: energy reaches 2; minerals reaches 1.
    actest::WorldFixture world;
    world.ctx.AddOccupantWithEffects(world.At(8, 8), "MixedRadius");

    const TileResources_t atOne = world.ctx.ResolveTileYield(world.At(9, 9)).effective;
    CHECK(atOne.energy == 1);
    CHECK(atOne.minerals == 1);

    const TileResources_t atTwo = world.ctx.ResolveTileYield(world.At(10, 10)).effective;
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

TEST_CASE("Aura collection: non-ThisTile effects do not leak into neighbors",
          "[effects][tile][aura]")
{
    // The neighbor-aura path applies the same filter as a tile's own features: ThisTile scope
    // only. WeirdAura (radius 1) carries a FactionGlobal +5 nutrients alongside its legitimate
    // ThisTile +1 energy; only the energy may reach the neighbor.
    actest::WorldFixture world;
    world.ctx.AddOccupantWithEffects(world.At(8, 8), "WeirdAura");

    const TileResources_t neighborYield = world.ctx.ResolveTileYield(world.At(9, 9)).effective;
    CHECK(neighborYield.energy == 1);    // the legitimate ThisTile aura effect
    CHECK(neighborYield.nutrients == 0); // FactionGlobal-scoped effect must not apply here
}

TEST_CASE("Sensor aura wraps horizontally across the map seam",
          "[effects][tile][aura][wrap][territory]")
{
    actest::FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    const int width = fixture.map.GetWidth();
    fixture.MakeFactionBase(owner, 4, 8);
    fixture.ctx.AddOccupantWithEffects(fixture.At(0, 4), "Sensor");
    Unit& defender = fixture.MakeUnit(owner, width - 2, 4, k_ArmoredLand);
    const Unit& attacker = fixture.MakeUnit(enemy, 8, 12, k_LandGun);

    REQUIRE(fixture.map.GetTerritory().GetOwner(0, 4) == owner.GetFactionId());
    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, width - 2, 4) == Approx(1.25));
    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, width - 4, 4) == Approx(1.25));
    CHECK(actest::ConventionalDefenseRatingAt(fixture, defender, attacker, width - 6, 4) == Approx(1.0));
}

TEST_CASE("Condenser moisture aura wraps horizontally across the map seam",
          "[effects][tile][moisture][wrap]")
{
    actest::WorldFixture world;
    const int width = world.map.GetWidth();
    Tile& dryAcrossSeam = world.At(width - 2, 4);
    dryAcrossSeam.SetBaseMoisture(Moisture_t::Arid);
    dryAcrossSeam.SetMoisture(Moisture_t::Arid);

    Tile& dryTooFar = world.At(width - 4, 4);
    dryTooFar.SetBaseMoisture(Moisture_t::Arid);
    dryTooFar.SetMoisture(Moisture_t::Arid);

    // Condenser radius 1 at the west edge reaches one tile across the wrap.
    world.ctx.AddOccupantWithEffects(world.At(0, 4), "Condenser");

    CHECK(dryAcrossSeam.GetMoisture() == Moisture_t::Moist);
    CHECK(dryAcrossSeam.GetBaseMoisture() == Moisture_t::Arid);
    CHECK(dryTooFar.GetMoisture() == Moisture_t::Arid);
}

TEST_CASE("Forest suppresses rockiness/moisture but keeps resource bonuses",
          "[effects][tile][yield][forest]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    world.ctx.AddOccupantWithEffects(tile, "Forest");
    world.ctx.AddOccupantWithEffects(tile, "Nutrients");

    const TileResources_t yield = world.ctx.ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 3);
    CHECK(yield.minerals == 2);
    CHECK(yield.energy == 1);
}

TEST_CASE("Fungus replaces terrain yield with Planet/tech inputs (bare fungus is 0/0/0)",
          "[effects][tile][yield][fungus]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    tile.SetHasRiver(true);
    world.ctx.AddOccupantWithEffects(tile, "Farm");
    world.ctx.AddOccupantWithEffects(tile, "Nutrients");
    world.ctx.AddOccupantWithEffects(tile, "MonsoonJungle");
    world.ctx.AddOccupantWithEffects(tile, "Fungus");

    // Suppresses landform/terraform/bonus/River/landmark; no Planet/tech inputs yet.
    const TileResources_t yield = world.ctx.ResolveTileYield(tile).effective;
    CHECK(yield.nutrients == 0);
    CHECK(yield.minerals == 0);
    CHECK(yield.energy == 0);
}

TEST_CASE("Fungus in deeper ocean has no effect until the floor reaches the shelf",
          "[effects][tile][yield][fungus]")
{
    actest::WorldFixture world;
    Tile& rFungus = world.At(8, 8);
    Tile& rBare = world.At(8, 4);
    const int deep = rFungus.MapRules().oceanShelfMeters - 1;
    rFungus.SetElevation(deep);
    rBare.SetElevation(deep);
    world.ctx.AddOccupantWithEffects(rFungus, "Fungus");

    const TileResources_t dormant = world.ctx.ResolveTileYield(rFungus).effective;
    const TileResources_t bare = world.ctx.ResolveTileYield(rBare).effective;
    CHECK(dormant.nutrients == bare.nutrients);
    CHECK(dormant.minerals == bare.minerals);
    CHECK(dormant.energy == bare.energy);

    // On the shelf the fungus takes over; without Planet/tech inputs it yields 0/0/0.
    rFungus.SetElevation(rFungus.MapRules().oceanShelfMeters);
    const TileResources_t awake = world.ctx.ResolveTileYield(rFungus).effective;
    CHECK(awake.nutrients == 0);
    CHECK(awake.minerals == 0);
    CHECK(awake.energy == 0);
}

TEST_CASE("Fungus yield can be boosted by base-effect selectors",
          "[effects][tile][yield][fungus][selector]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    fixture.ctx.AddOccupantWithEffects(tile, "Fungus");

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "fungus_booster"),
    }};

    const TileResources_t yield = fixture.ctx.ResolveTileYield(tile, false, baseEffects).effective;
    CHECK(yield.nutrients == 1);
    CHECK(yield.minerals == 0);
    CHECK(yield.energy == 0);
}

TEST_CASE("ResolveTileYield floors each resource at 0",
          "[effects][tile][yield]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, -5.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::AnyTileSelector()), "penalty"),
    }};

    CHECK(fixture.ctx.ResolveTileYield(tile, false, baseEffects).effective.nutrients == 0);
}

TEST_CASE("Fungus yield stacks Planet clamp contribution with tech fungus Adds",
          "[effects][tile][yield][fungus]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);
    fixture.ctx.AddOccupantWithEffects(tile, "Fungus");

    actest::EffectPool pool;
    // Planet −2 on fungus + Ecology-style +1 nutrient → floor at 0.
    const BaseEffects_t planetOnly{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, -2.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "planet"),
        actest::Active(pool.StatMod(StatId_t::Minerals, -2.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "planet"),
        actest::Active(pool.StatMod(StatId_t::Energy, -2.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "planet"),
    }};
    CHECK(fixture.ctx.ResolveTileYield(tile, false, planetOnly).effective.nutrients == 0);

    const BaseEffects_t planetAndTech{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, -2.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "planet"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "ecology"),
        actest::Active(pool.StatMod(StatId_t::Minerals, -2.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "planet"),
        actest::Active(pool.StatMod(StatId_t::Energy, -2.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "planet"),
    }};
    CHECK(fixture.ctx.ResolveTileYield(tile, false, planetAndTech).effective.nutrients == 0);

    const BaseEffects_t techOnly{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "ecology"),
    }};
    CHECK(fixture.ctx.ResolveTileYield(tile, false, techOnly).effective.nutrients == 1);
}

TEST_CASE("Fungus suppresses tile_yield_rules MaxClamp so fungus yield is uncapped",
          "[effects][tile][yield][fungus]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);
    fixture.ctx.AddOccupantWithEffects(tile, "Fungus");

    actest::EffectPool pool;
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "ecology"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "centauri_psi"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::ImprovementSelector("Fungus")), "fung_nutrient"),
        actest::Active(pool.StatMod(StatId_t::Nutrients, 2.0, ModifierOp_t::MaxClamp,
                                    EffectScope_t::FactionGlobal, actest::AnyTileSelector()),
                       "tile_yield_rules"),
    }};

    const TileYieldView_t yield = fixture.ctx.ResolveTileYield(tile, false, baseEffects);
    CHECK(yield.effective.nutrients == 3);
    CHECK(yield.potential.nutrients == 3);
}

TEST_CASE("Economy AnyTile energy skips fungus tiles via TargetTileHas present false",
          "[effects][tile][yield][fungus][economy]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& fungus = fixture.At(8, 8);
    Tile& clear = fixture.At(9, 9);
    fixture.ctx.AddOccupantWithEffects(fungus, "Fungus");

    actest::EffectPool pool;
    const Condition_t noFungus = TargetTileHas_t{"Fungus", false};
    const BaseEffects_t baseEffects{base, {
        actest::Active(pool.StatMod(StatId_t::Energy, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                                    actest::AnyTileSelector(), noFungus),
                       "se_rating_economy_2"),
    }};

    CHECK(fixture.ctx.ResolveTileYield(fungus, false, baseEffects).effective.energy == 0);
    CHECK(fixture.ctx.ResolveTileYield(clear, false, baseEffects).effective.energy == 1);
}

TEST_CASE("Monolith replaces terrain yield, keeps river energy, and stacks resource bonuses",
          "[effects][tile][yield][monolith]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(8, 8);
    tile.SetRockiness(Rockiness_t::Rocky);
    tile.SetMoisture(Moisture_t::Wet);
    tile.SetHasRiver(true);
    world.ctx.AddOccupantWithEffects(tile, "Farm");
    world.ctx.AddOccupantWithEffects(tile, "Nutrients");
    world.ctx.AddOccupantWithEffects(tile, "Monolith");

    const TileResources_t yield = world.ctx.ResolveTileYield(tile).effective;
    // Monolith 2 + Nutrients 2; landform/farm suppressed.
    CHECK(yield.nutrients == 4);
    CHECK(yield.minerals == 2);
    // Monolith 2 + River 1; bare resolve has no faction MaxClamp.
    CHECK(yield.energy == 3);
}

TEST_CASE("Minerals grants +1 with Mine and Road",
          "[effects][tile][yield][mine]")
{
    actest::WorldFixture world;
    Tile& withRoad = world.At(8, 8);
    Tile& withoutRoad = world.At(9, 9);
    world.ctx.AddOccupantWithEffects(withRoad, "Minerals");
    world.ctx.AddOccupantWithEffects(withRoad, "Mine");
    world.ctx.AddOccupantWithEffects(withRoad, "Road");
    world.ctx.AddOccupantWithEffects(withoutRoad, "Minerals");
    world.ctx.AddOccupantWithEffects(withoutRoad, "Mine");

    // Fixture Mine +2; Minerals +2; Minerals' Mine+Road term +1.
    CHECK(world.ctx.ResolveTileYield(withRoad).effective.minerals == 5);
    CHECK(world.ctx.ResolveTileYield(withoutRoad).effective.minerals == 4);
}

TEST_CASE("TerraformSpreadGrowthAttempts matches SMAC alien_fauna formula",
          "[terraform][spread]")
{
    CHECK(TerraformSpreadGrowthAttempts(3200, 0) == 100);   // 3200 / 32
    CHECK(TerraformSpreadGrowthAttempts(3200, 100) == 56);  // 3200 / (25 + 32)
    CHECK(TerraformSpreadGrowthAttempts(0, 0) == 0);
}

TEST_CASE("TrySpreadTerraformFromTile spreads Forest onto open land and leaves fungus",
          "[terraform][spread]")
{
    actest::WorldFixture world;
    Tile& origin = world.At(8, 8);
    origin.SetElevation(100);
    origin.SetRockiness(Rockiness_t::Flat);
    world.ctx.AddOccupantWithEffects(origin, "Forest");

    // Fungus blocks forest. The open wet tile is the legal neighbor.
    ForEachNeighborInChebyshevRadius(origin, world.map, 1,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            pNeighbor->SetElevation(-1000); // sea: ineligible for Forest
        });

    Tile& arid = world.At(9, 9);
    arid.SetElevation(100);
    arid.SetRockiness(Rockiness_t::Flat);
    arid.SetMoisture(Moisture_t::Arid);
    world.ctx.AddOccupantWithEffects(arid, "Fungus");

    Tile& wet = world.At(7, 7);
    wet.SetElevation(100);
    wet.SetRockiness(Rockiness_t::Flat);
    wet.SetMoisture(Moisture_t::Wet);

    REQUIRE(TrySpreadTerraformFromTile(origin, world.map, world.ctx));
    CHECK_FALSE(arid.HasImprovement("Forest"));
    CHECK(arid.HasFeature("Fungus"));
    CHECK(wet.HasImprovement("Forest"));
}

TEST_CASE("TrySpreadTerraformFromTile does not spread Forest onto Rocky tiles",
          "[terraform][spread]")
{
    actest::WorldFixture world;
    Tile& origin = world.At(8, 8);
    origin.SetElevation(100);
    world.ctx.AddOccupantWithEffects(origin, "Forest");

    ForEachNeighborInChebyshevRadius(origin, world.map, 1,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            pNeighbor->SetElevation(100);
            pNeighbor->SetRockiness(Rockiness_t::Rocky);
            pNeighbor->SetMoisture(Moisture_t::Arid);
        });

    CHECK_FALSE(TrySpreadTerraformFromTile(origin, world.map, world.ctx));
    ForEachNeighborInChebyshevRadius(origin, world.map, 1,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            CHECK_FALSE(pNeighbor->HasImprovement("Forest"));
        });
}

TEST_CASE("SpreadTerraformImprovements eventually spreads from a sampled Forest",
          "[terraform][spread]")
{
    actest::WorldFixture world;
    Tile& origin = world.At(8, 8);
    origin.SetElevation(100);
    origin.SetRockiness(Rockiness_t::Flat);
    world.ctx.AddOccupantWithEffects(origin, "Forest");

    Tile& neighbor = world.At(9, 9);
    neighbor.SetElevation(100);
    neighbor.SetRockiness(Rockiness_t::Flat);
    neighbor.SetMoisture(Moisture_t::Arid);

    // Leave other adjacent tiles rocky so only `neighbor` can receive the spread.
    ForEachNeighborInChebyshevRadius(origin, world.map, 1,
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
        SpreadTerraformImprovements(world.map, world.ctx, /*turnIndex=*/0, rng);
        spread = neighbor.HasImprovement("Forest");
    }
    CHECK(spread);
}

TEST_CASE("Forest spread prefers the arid, flat neighbour and changes nothing while choosing",
          "[terraform][spread]")
{
    actest::WorldFixture world;
    Tile& origin = world.At(8, 8);
    origin.SetElevation(100);
    world.ctx.AddOccupantWithEffects(origin, "Forest");
    ForEachNeighborInChebyshevRadius(origin, world.map, 1,
        [&](Tile* pNeighbor, int /*distance*/)
        {
            pNeighbor->SetElevation(-1000);
        });

    Tile& wet = world.At(7, 7);
    wet.SetElevation(100);
    wet.SetRockiness(Rockiness_t::Flat);
    wet.SetMoisture(Moisture_t::Wet);
    Tile& arid = world.At(9, 9);
    arid.SetElevation(100);
    arid.SetRockiness(Rockiness_t::Flat);
    arid.SetMoisture(Moisture_t::Arid);

    const std::optional<TerraformSpread_t> spread = PickSpreadTarget(origin);
    REQUIRE(spread.has_value());
    CHECK(spread->pTarget == &arid);
    CHECK(spread->pImprovement->id == "Forest");
    CHECK_FALSE(arid.HasImprovement("Forest"));
}
