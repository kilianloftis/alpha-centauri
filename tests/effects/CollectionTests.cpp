// Tests for effect collection: AppendActiveEffects, CollectUnitEffects, CollectPopEffects,
// CollectFromPops, and CollectTileEffects.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/faction/base/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/units/UnitComponentConfig.h"
#include "game/effects/ActiveEffect.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

int CountBySource(const std::vector<ActiveEffect_t>& effects, const std::string& sourceId)
{
    int count = 0;
    for (const ActiveEffect_t& effect : effects)
    {
        if (effect.sourceId == sourceId)
        {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("AppendActiveEffects: skips Instantaneous effects", "[effects][collect]")
{
    actest::EffectPool pool;
    std::vector<EffectConfig_t> configs = {
        pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase),
        pool.StatMod(StatId::Nutrients, 5.0, ModifierOp::Add, EffectScope_t::ThisBase,
                     std::nullopt, std::nullopt, EffectPersistence_t::Instantaneous),
    };

    std::vector<ActiveEffect_t> out;
    AppendActiveEffects(configs, nullptr, "source", out);

    REQUIRE(out.size() == 1);
    CHECK(out[0].config == &configs[0]);
    CHECK(out[0].sourceId == "source");
}

TEST_CASE("AppendActiveEffects: originBase is recorded only for ThisBase-scoped effects", "[effects][collect]")
{
    actest::BaseFixture fixture;
    const BaseManager& base = fixture.MakeBase(4, 4);

    actest::EffectPool pool;
    std::vector<EffectConfig_t> configs = {
        pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase),
        pool.StatMod(StatId::Energy, 1.0, ModifierOp::Add, EffectScope_t::FactionGlobal),
        pool.StatMod(StatId::Minerals, 1.0, ModifierOp::Add, EffectScope_t::AllOwnerBases),
    };

    std::vector<ActiveEffect_t> out;
    AppendActiveEffects(configs, &base, "source", out);

    REQUIRE(out.size() == 3);
    CHECK(out[0].originBase == &base);     // ThisBase
    CHECK(out[1].originBase == nullptr);   // FactionGlobal
    CHECK(out[2].originBase == nullptr);   // AllOwnerBases
}

TEST_CASE("CollectUnitEffects: gathers component effects, tagged with the component id",
          "[effects][collect][unit]")
{
    actest::EffectPool pool;

    UnitComponentConfig_t weapon;
    weapon.id = "laser";
    weapon.effects = {pool.StatMod(StatId::Attack, 4.0, ModifierOp::Add, EffectScope_t::ThisUnit)};

    UnitComponentConfig_t chassis;
    chassis.id = "speeder";
    chassis.effects = {
        pool.StatMod(StatId::Movement, 2.0, ModifierOp::Add, EffectScope_t::ThisUnit),
        pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, std::nullopt, EffectPersistence_t::Instantaneous),
    };

    const std::vector<const UnitComponentConfig_t*> components = {&weapon, nullptr, &chassis};
    const std::vector<ActiveEffect_t> effects = CollectUnitEffects(components);

    // Null components are skipped, Instantaneous effects are skipped.
    REQUIRE(effects.size() == 2);
    CHECK(effects[0].sourceId == "laser");
    CHECK(effects[1].sourceId == "speeder");
    CHECK(effects[0].originBase == nullptr);
}

TEST_CASE("CollectPopEffects: gathers all scopes of a pop type's effects, tagged with the type id",
          "[effects][collect][pop]")
{
    actest::PopTypeRegistryOnly reg;
    const PopTypeConfig_t* pFarmer = reg.popTypes.Find("FungalFarmer");
    REQUIRE(pFarmer != nullptr);

    const std::vector<ActiveEffect_t> effects = CollectPopEffects(*pFarmer);

    // Both the ThisPop tile multiplier and the ThisBase flat bonus are collected here;
    // the split happens later via FilterByScope.
    REQUIRE(effects.size() == 2);
    CHECK(effects[0].sourceId == "FungalFarmer");
    CHECK(std::ranges::distance(FilterByScope(effects, EffectScope_t::ThisPop)) == 1);
    CHECK(std::ranges::distance(FilterByScope(effects, EffectScope_t::ThisBase)) == 1);
}

TEST_CASE("CollectFromPops: only ThisBase-scoped pop effects enter the base pool, tagged with the base",
          "[effects][collect][pop]")
{
    actest::BaseFixture fixture;
    const BaseManager& base = fixture.MakeBase(4, 4);

    PopulationManager pops(fixture.dataContext.popTypeRegistry.get(),
                           fixture.dataContext.popTypeAvailabilityCalculator.get(),
                           fixture.dataContext.growthConfig.get(),
                           fixture.dataContext.popCompositionCalculator.get(),
                           /*research*/ nullptr, 0);
    pops.AddPop("Doctor");
    pops.AddPop("Doctor");
    pops.AddPop("Technician");
    pops.AddPop("FungalFarmer");
    pops.AddPop("Worker"); // no effects at all

    const std::vector<ActiveEffect_t> effects = CollectFromPops(pops, base);

    // 2 Doctors (+2 psych each), 1 Technician (+3 econ), FungalFarmer's ThisBase +1 econ.
    // The FungalFarmer's ThisPop tile multiplier must NOT appear.
    REQUIRE(effects.size() == 4);
    for (const ActiveEffect_t& effect : effects)
    {
        CHECK(effect.config->scope == EffectScope_t::ThisBase);
        CHECK(effect.originBase == &base);
    }

    // Stacking: two Doctors contribute +4 psych in total.
    CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Psych), 0.0).total == 4.0);
    CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Econ), 0.0).total == 4.0);
    CHECK(CountBySource(effects, "Doctor") == 2);
}

TEST_CASE("CollectTileEffects: terrain features resolve by string id through the registry",
          "[effects][collect][tile]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(2, 2);

    SECTION("featureless terrain (Flat/Arid) yields no effects")
    {
        const auto effects = CollectTileEffects(tile, world.improvements);
        CHECK(effects.empty());
    }

    SECTION("Rocky contributes its mineral and defense effects, sourceId = feature id")
    {
        tile.SetRockiness(Rockiness::Rocky);
        const auto effects = CollectTileEffects(tile, world.improvements);
        REQUIRE(effects.size() == 2);
        CHECK(effects[0].sourceId == "Rocky");
        CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Minerals), 0.0).total == 2.0);
    }

    SECTION("river and moisture stack with rockiness")
    {
        tile.SetRockiness(Rockiness::Rolling); // +1 mineral
        tile.SetMoisture(Moisture::Wet);       // +2 nutrients
        tile.SetHasRiver(true);                // +1 energy
        const auto effects = CollectTileEffects(tile, world.improvements);
        CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Minerals), 0.0).total == 1.0);
        CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Nutrients), 0.0).total == 2.0);
        CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Energy), 0.0).total == 1.0);
    }
}

TEST_CASE("CollectTileEffects: improvements contribute directly from their held configs",
          "[effects][collect][tile]")
{
    actest::WorldFixture world;
    Tile& tile = world.At(2, 2);

    const ImprovementConfig_t* pFarm = world.improvements.Find("Farm");
    const ImprovementConfig_t* pMine = world.improvements.Find("Mine");
    REQUIRE(pFarm != nullptr);
    REQUIRE(pMine != nullptr);

    tile.AddImprovement(*pFarm);
    tile.AddImprovement(*pMine);

    const auto effects = CollectTileEffects(tile, world.improvements);
    CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Nutrients), 0.0).total == 1.0);
    CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Minerals), 0.0).total == 2.0);
    CHECK(CountBySource(effects, "Farm") == 1);
    CHECK(CountBySource(effects, "Mine") == 1);
}

TEST_CASE("CollectTileEffects: only ThisTile-scoped effects are collected from a tile's own features",
          "[effects][collect][tile]")
{
    // WeirdAura carries a ThisBase-scoped effect; that must never enter tile-local resolution.
    actest::WorldFixture world;
    Tile& tile = world.At(2, 2);
    tile.AddImprovement(*world.improvements.Find("WeirdAura"));

    const auto effects = CollectTileEffects(tile, world.improvements);
    for (const ActiveEffect_t& effect : effects)
    {
        CHECK(effect.config->scope == EffectScope_t::ThisTile);
    }
    CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Nutrients), 0.0).total == 0.0);
}

TEST_CASE("CollectTileEffects: Instantaneous effects do not enter the continuous tile pool",
          "[effects][collect][tile]")
{
    // Same rule as AppendActiveEffects (buildings/pops/units): Instantaneous effects fire
    // once when applied, never as part of continuous resolution. WeirdAura declares an
    // Instantaneous ThisTile +7 minerals effect that must be ignored here.
    actest::WorldFixture world;
    Tile& tile = world.At(2, 2);
    tile.AddImprovement(*world.improvements.Find("WeirdAura"));

    const auto effects = CollectTileEffects(tile, world.improvements);
    CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Minerals), 0.0).total == 0.0);
}
