// Integration tests through real BaseManager instances: base-scoped effect attribution,
// FilterForBase, instantaneous dispatch, and the full resource-production pipeline.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/buildings/BuildingConfigParser.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"
#include "lib/effects/ActiveEffect.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using Catch::Approx;

TEST_CASE("Founding a base registers the Base improvement and its defense bonus on the tile",
          "[effects][base]")
{
    actest::BaseFixture fixture;
    fixture.MakeBase(4, 4);

    Tile& tile = fixture.At(4, 4);
    CHECK(tile.HasImprovement("Base"));
    CHECK(tile.HasFeature("Base"));
    // Fixture Base entry: +100% defense through the same mechanism as Bunker/Rocky.
    CHECK(fixture.ctx->ResolveTileDefenseMultiplier(tile) == Approx(2.0));
}

TEST_CASE("BaseManager::CollectBuildingEffects tags ThisBase effects with the owning base",
          "[effects][base]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);
    BaseManager& baseB = fixture.MakeBase(6, 6);

    baseA.AddBuilding("flat_nutrient");
    baseB.AddBuilding("granted_hall");

    const auto effectsA = baseA.CollectBuildingEffects();
    REQUIRE(effectsA.size() == 1);
    CHECK(effectsA[0].sourceId == "flat_nutrient");
    CHECK(effectsA[0].originBase == &baseA);

    // granted_hall: ThisBase minerals -> tagged with baseB; FactionGlobal energy -> untagged.
    const auto effectsB = baseB.CollectBuildingEffects();
    REQUIRE(effectsB.size() == 2);
    for (const ActiveEffect_t& effect : effectsB)
    {
        if (effect.config->scope == EffectScope_t::ThisBase)
        {
            CHECK(effect.originBase == &baseB);
        }
        else
        {
            CHECK(effect.originBase == nullptr);
        }
    }
}

TEST_CASE("FilterForBase: scope rules with real base identities", "[effects][base][filter]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);
    BaseManager& baseB = fixture.MakeBase(6, 6);

    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        actest::Active(pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase),
                       "mine_a", &baseA),
        actest::Active(pool.StatMod(StatId::Nutrients, 2.0, ModifierOp::Add, EffectScope_t::ThisBase),
                       "theirs_b", &baseB),
        actest::Active(pool.StatMod(StatId::Nutrients, 4.0, ModifierOp::Add, EffectScope_t::AllOwnerBases),
                       "all_bases"),
        actest::Active(pool.StatMod(StatId::Nutrients, 8.0, ModifierOp::Add, EffectScope_t::FactionGlobal),
                       "faction"),
        actest::Active(pool.StatMod(StatId::Nutrients, 16.0, ModifierOp::Add, EffectScope_t::WorldGlobal),
                       "world"),
        actest::Active(pool.StatMod(StatId::Nutrients, 32.0, ModifierOp::Add, EffectScope_t::ThisUnit),
                       "unit"),
        actest::Active(pool.StatMod(StatId::Nutrients, 64.0, ModifierOp::Add, EffectScope_t::FactionUnits),
                       "faction_units"),
        actest::Active(pool.StatMod(StatId::Nutrients, 128.0, ModifierOp::Add, EffectScope_t::ThisPop),
                       "pop"),
        actest::Active(pool.StatMod(StatId::Nutrients, 256.0, ModifierOp::Add, EffectScope_t::ThisTile),
                       "tile"),
    };

    // For base A: its own ThisBase effect plus the three base-applicable global scopes.
    // The distinct powers of two make any wrong inclusion identifiable from the total.
    const auto forA = FilterForBase(effects, baseA);
    CHECK(ResolveStatModifiers(FilterByStatId(forA, StatId::Nutrients), 0.0).total == Approx(1.0 + 4.0 + 8.0 + 16.0));

    const auto forB = FilterForBase(effects, baseB);
    CHECK(ResolveStatModifiers(FilterByStatId(forB, StatId::Nutrients), 0.0).total == Approx(2.0 + 4.0 + 8.0 + 16.0));
}

TEST_CASE("FilterForBase: a ThisBase effect with no origin base applies to no base", "[effects][base][filter]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        actest::Active(pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase),
                       "orphan", nullptr),
    };
    CHECK(FilterForBase(effects, baseA).empty());
}

TEST_CASE("DispatchInstantaneousEffects: Instantaneous GrantBuilding constructs the building immediately",
          "[effects][base][instantaneous]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);

    const BuildingConfig_t* pGrantor = fixture.buildings().Find("instant_grantor");
    REQUIRE(pGrantor != nullptr);

    // Simulate on_production_completed for instant_grantor.
    base.AddBuilding(pGrantor->id);
    DispatchInstantaneousEffects(*pGrantor, base);

    bool hasGranted = false;
    for (const BuildingConfig_t* pBuilding : base.GetBuildings())
    {
        if (pBuilding->id == "flat_nutrient")
        {
            hasGranted = true;
        }
    }
    CHECK(hasGranted);

    // The granted building is a real constructed building: its continuous effects collect.
    const auto effects = base.CollectBuildingEffects();
    CHECK(ResolveStatModifiers(FilterByStatId(effects, StatId::Nutrients), 0.0).total == 2.0);
}

TEST_CASE("Full pipeline: building and pop bonuses land in base resource production",
          "[effects][base][pipeline]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);

    // A wet farm tile next door: Wet(+2) + Farm(+1) nutrients.
    Tile& farmTile = fixture.At(5, 4);
    farmTile.SetBaseMoisture(Moisture::Wet);
    farmTile.SetMoisture(Moisture::Wet);
    fixture.ctx->AddImprovementWithEffects(farmTile, "Farm");

    // Buildings: +2 flat nutrients, +1 nutrients on each worked Farm.
    base.AddBuilding("flat_nutrient");
    base.AddBuilding("farm_booster");

    // One of the three starting Workers works the farm tile; another becomes a Doctor
    // (+2 psych). Workers are auto-assigned at base construction, so pick any pop that is
    // not working the farm — the tiles the others work are all barren (zero yield).
    base.UserAssignBestAvailableWorker(&farmTile);
    auto& pops = const_cast<PopContainer&>(base.GetPopContainer()).GetPops();
    REQUIRE(pops.size() == 3);
    Pop* pOtherWorker = nullptr;
    for (auto& pPop : pops)
    {
        if (pPop->GetTile() != &farmTile)
        {
            pOtherWorker = pPop.get();
            break;
        }
    }
    REQUIRE(pOtherWorker != nullptr);
    base.ConvertPop(*pOtherWorker, "Doctor");

    base.ProduceResources(base.CollectBuildingEffects());

    // Nutrients: farm tile (2 Wet + 1 Farm + 1 booster) + base center tile (0) + flat 2 = 6.
    CHECK(base.GetNutrientProduction() == 6);
    // Minerals: nothing anywhere.
    CHECK(base.GetMineralProduction() == 0);
    // Psych: no energy to split (all elevation 0), so exactly the Doctor's +2.
    CHECK(base.GetPsychProduction() == 2);
}
