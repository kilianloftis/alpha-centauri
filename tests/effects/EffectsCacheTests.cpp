// Tests for the derived-state seam (code review finding 1.1): the faction effect pool is
// memoized and revalidated against per-subsystem Revision counters, and derived caches
// (base effects, research cost) key on the pool version.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/IEffectsProvider.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/units/Unit.h"
#include "lib/LuaRuntime.h"
#include "game/effects/ActiveEffect.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using Catch::Approx;

TEST_CASE("Effect pool cache: stable across reads, invalidated by every contributor",
          "[effects][cache]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    // Repeated reads neither rebuild nor move the pool.
    const uint64_t v0 = faction.GetEffectsVersion();
    const FactionEffects_t* pPool = &faction.GetActiveEffects();
    CHECK(faction.GetEffectsVersion() == v0);
    CHECK(&faction.GetActiveEffects() == pPool);

    // Buildings invalidate.
    base.GetBuildingManager().AddBuilding("flat_nutrient");
    const uint64_t v1 = faction.GetEffectsVersion();
    CHECK(v1 != v0);
    base.GetBuildingManager().DestroyBuilding("flat_nutrient");
    const uint64_t v2 = faction.GetEffectsVersion();
    CHECK(v2 != v1);

    // Pops invalidate, and the rebuilt pool carries the new content:
    // a Networker contributes "+1 labs, FactionGlobal".
    base.GetPopulation().AddPop("Networker");
    const uint64_t v3 = faction.GetEffectsVersion();
    CHECK(v3 != v2);
    CHECK(ResolveStatModifiers(FilterByStatId(faction.GetActiveEffects().effects, StatId::Labs), 0.0).total
          == Approx(1.0));

    // Social policies invalidate.
    REQUIRE(faction.GetSocialEngineering().SetActivePolicy(SocialCategory::Economics, "wealth_policy"));
    const uint64_t v4 = faction.GetEffectsVersion();
    CHECK(v4 != v3);

    // Units invalidate on creation and destruction.
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"energy_siphon"});
    const uint64_t v5 = faction.GetEffectsVersion();
    CHECK(v5 != v4);
    faction.GetUnitManager().DestroyUnit(unit);
    const uint64_t v6 = faction.GetEffectsVersion();
    CHECK(v6 != v5);

    // Founding another base invalidates (base-list revision).
    fixture.MakeFactionBase(faction, 6, 6);
    CHECK(faction.GetEffectsVersion() != v6);
}

TEST_CASE("Base effects cache: stat getters stay current through the memoized path",
          "[effects][cache]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    const int before = base.GetNutrientProduction();
    CHECK(base.GetNutrientProduction() == before); // repeated read via the cache

    base.GetBuildingManager().AddBuilding("flat_nutrient"); // +2 flat nutrients
    CHECK(base.GetNutrientProduction() == before + 2);

    base.GetBuildingManager().DestroyBuilding("flat_nutrient");
    CHECK(base.GetNutrientProduction() == before);
}

namespace
{

// Minimal provider double with an externally mutable pool + version.
struct FakeEffectsProvider : ac::IEffectsProvider
{
    ac::FactionEffects_t pool;
    uint64_t version = 1;

    const ac::FactionEffects_t& GetActiveEffects() const override { return pool; }
    uint64_t GetEffectsVersion() const override { return version; }
};

} // namespace

TEST_CASE("ResearchManager: a TechCost effect appearing mid-research updates the needed points",
          "[research][cache]")
{
    TechRegistry techRegistry;
    techRegistry.Load(actest::FixturePath("techs.json"));
    LuaRuntime lua;
    TechCostConfigParser parser;
    const TechCostConfig config =
        parser.ParseConfig(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/tech_cost.lua", lua);
    TechCostCalculator calculator(config, lua);

    FakeEffectsProvider provider;
    ResearchManager research(&techRegistry, &calculator, &provider);

    research.SetResearchTarget("build_tech");
    const int baseCost = research.GetPointsNeededForCurrentTech();

    // A +50% TechCost modifier appears mid-research (e.g. from a newly built project).
    actest::EffectPool effectPool;
    provider.pool.effects.push_back(actest::Active(
        effectPool.StatMod(StatId::TechCost, 50.0, ModifierOp::Add), "tech_tax"));
    ++provider.version;

    const int raisedCost = research.GetPointsNeededForCurrentTech();
    CHECK(raisedCost > baseCost);

    // CanDiscoverTech uses the revalidated cost: points that met the old cost no
    // longer meet the raised one.
    research.SetAccumulatedPoints(baseCost);
    CHECK_FALSE(research.CanDiscoverTech());
    research.SetAccumulatedPoints(raisedCost);
    CHECK(research.CanDiscoverTech());
}
