// Tests for the derived-state seam (code review finding 1.1): the faction effect pool is
// memoized and revalidated against per-subsystem Revision counters, and derived caches
// (base effects, research cost) key on the pool version.

#include "GameFixtures.h"
#include "TempConfigFile.h"
#include "TestHelpers.h"

#include "game/effects/IEffectsProvider.h"
#include "game/research/ResearchManager.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/units/Unit.h"
#include "lib/LuaRuntime.h"
#include "game/effects/ActiveEffect.h"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using Catch::Approx;
using actest::TempConfigFile;

TEST_CASE("Effect pool cache: stable across reads, invalidated by every contributor",
          "[effects][cache]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 4);

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
    CHECK(ResolveStatModifiers(FilterByStatId(faction.GetActiveEffects().effects, StatId_t::Labs), 0.0, nullptr).total
          == Approx(1.0));

    // Social policies invalidate.
    faction.GetSocialEngineering().SetActivePolicy(fixture.socialPolicies().Get("wealth_policy"));
    const uint64_t v4 = faction.GetEffectsVersion();
    CHECK(v4 != v3);

    // Units invalidate on creation and destruction.
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis", "energy_siphon"});
    const uint64_t v5 = faction.GetEffectsVersion();
    CHECK(v5 != v4);
    faction.GetUnitManager().DestroyUnit(unit);
    const uint64_t v6 = faction.GetEffectsVersion();
    CHECK(v6 != v5);

    // Founding another base invalidates (base-list revision).
    fixture.MakeFactionBase(faction, 8, 12);
    CHECK(faction.GetEffectsVersion() != v6);
}

TEST_CASE("Base effects cache: stat getters stay current through the memoized path",
          "[effects][cache]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

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
    explicit FakeEffectsProvider(const ac::Faction& rFaction)
        : pool(rFaction)
    {
    }

    ac::FactionEffects_t pool;
    uint64_t version = 1;

    const ac::FactionEffects_t& GetActiveEffects() const override { return pool; }
    uint64_t GetEffectsVersion() const override { return version; }
    // No world extras on the fake, so the composed and local views are the same list.
    const ac::FactionEffects_t& GetLocalActiveEffects() const override { return pool; }
};

} // namespace

TEST_CASE("ResearchManager: a TechCost effect appearing mid-research updates the needed points",
          "[research][cache]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    TechRegistry techRegistry;
    techRegistry.Load(actest::FixturePath("techs.json"));
    LuaRuntime lua;
    TechCostConfigParser parser;
    const TechCostConfig_t config =
        parser.ParseConfig(actest::FixturePath("tech_cost.lua"), lua);
    TechCostCalculator calculator(config, lua);

    FakeEffectsProvider provider(faction);
    ResearchManager research(techRegistry, calculator, &provider);

    research.SetResearchTarget("build_tech");
    const int baseCost = research.GetPointsNeededForCurrentTech();

    // A 1.5x TechCost modifier appears mid-research (e.g. from a newly built project).
    actest::EffectPool effectPool;
    provider.pool.effects.push_back(actest::Active(
        effectPool.StatMod(StatId_t::TechCost, 1.5, ModifierOp_t::MultiplyGeometric), "tech_tax"));
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

TEST_CASE("ResearchManager: before the first breakthrough, cost follows the live tech count",
          "[research][cache]")
{
    TechRegistry techRegistry;
    techRegistry.Load(actest::FixturePath("techs.json"));
    LuaRuntime lua;
    TechCostConfigParser parser;
    const TechCostConfig_t config =
        parser.ParseConfig(actest::FixturePath("tech_cost.lua"), lua);
    TechCostCalculator calculator(config, lua);

    ResearchManager research(techRegistry, calculator, /*pEffectsProvider*/ nullptr);
    research.AddDiscoveredTech("build_tech");
    research.SetResearchTarget("advanced_build");
    const int costBefore = research.GetPointsNeededForCurrentTech();

    research.AddDiscoveredTech("grow_tech");
    CHECK(research.GetPointsNeededForCurrentTech() != costBefore);
}

TEST_CASE("ResearchManager: a breakthrough fixes the formula cost until the next one",
          "[research][cache]")
{
    TechRegistry techRegistry;
    techRegistry.Load(actest::FixturePath("techs.json"));
    LuaRuntime lua;
    TechCostConfigParser parser;
    const TechCostConfig_t config =
        parser.ParseConfig(actest::FixturePath("tech_cost.lua"), lua);
    TechCostCalculator calculator(config, lua);

    ResearchManager research(techRegistry, calculator, /*pEffectsProvider*/ nullptr);
    research.SetResearchTarget("build_tech");
    research.SetAccumulatedPoints(research.GetPointsNeededForCurrentTech() + 7);
    REQUIRE(research.DiscoverTech());
    CHECK(research.GetAccumulatedPoints() == 0);

    research.SetResearchTarget("advanced_build");
    const int frozenCost = research.GetPointsNeededForCurrentTech();
    // A traded or stolen tech is not a breakthrough: terranx keeps the cached cost.
    research.AddDiscoveredTech("grow_tech");
    CHECK(research.GetPointsNeededForCurrentTech() == frozenCost);

    research.SetAccumulatedPoints(frozenCost);
    REQUIRE(research.DiscoverTech());
    research.SetResearchTarget("discover_tech");
    CHECK(research.GetPointsNeededForCurrentTech() != frozenCost);
}

TEST_CASE("ResearchManager: ThisTech TechCost raises research cost without entering the pool",
          "[research][this-tech]")
{
    const TempConfigFile techs("ac_this_tech_cost.json", R"([
        { "id": "plain", "name": "Plain", "category": "build" },
        {
          "id": "pricey",
          "name": "Pricey",
          "category": "build",
          "effects": [
            {
              "type": "StatModifier",
              "scope": "ThisTech",
              "parameters": { "stat": "tech_cost", "amount": 1.5, "op": "MultiplyGeometric" }
            }
          ]
        }
    ])");

    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    TechRegistry techRegistry;
    techRegistry.Load(techs.Path());
    LuaRuntime lua;
    TechCostConfigParser parser;
    const TechCostConfig_t config =
        parser.ParseConfig(actest::FixturePath("tech_cost.lua"), lua);
    TechCostCalculator calculator(config, lua);

    FakeEffectsProvider provider(faction);
    ResearchManager research(techRegistry, calculator, &provider);

    research.SetResearchTarget("plain");
    const int plainCost = research.GetPointsNeededForCurrentTech();

    research.SetResearchTarget("pricey");
    const int priceyCost = research.GetPointsNeededForCurrentTech();
    CHECK(priceyCost > plainCost);
    CHECK(priceyCost == static_cast<int>(std::trunc(static_cast<double>(plainCost) * 1.5)));

    const int factionTechCostBefore = FinalizeResolvedStat(
        ResolveFactionStat(provider.pool, StatId_t::TechCost, SeedFor(StatId_t::TechCost), nullptr));
    research.AddDiscoveredTech("pricey");
    // Rebuild would normally pull discovered tech effects; ThisTech must not leak TechCost.
    std::vector<ActiveEffect_t> discovered;
    const TechConfig_t* pPricey = techRegistry.Find("pricey");
    REQUIRE(pPricey != nullptr);
    AppendFactionLaneEffects(pPricey->effects, nullptr, pPricey->id, discovered);
    CHECK(discovered.empty());
    CHECK(FinalizeResolvedStat(ResolveFactionStat(provider.pool, StatId_t::TechCost,
                                                   SeedFor(StatId_t::TechCost), nullptr))
          == factionTechCostBefore);
}
