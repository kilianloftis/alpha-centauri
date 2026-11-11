#include "game/CurrentGameState.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchRules.h"
#include "game/research/ResearchSelector.h"
#include "game/GameCategory.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/faction/Faction.h"
#include "game/map/WorldMap.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "game/effects/EffectConfig.h"
#include "game/social-engineering/SocialEngineeringRules.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "lib/LuaRuntime.h"

#include "GameFixtures.h"
#include "TempConfigFile.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

using namespace ac;
using namespace actest;

namespace
{

struct ResearchTestFixture
{
    TechRegistry techRegistry;
    std::unique_ptr<LuaRuntime> luaRuntime;
    std::unique_ptr<TechCostConfig_t> techCostConfig;
    std::unique_ptr<TechCostCalculator> techCostCalculator;
    std::unique_ptr<ResearchManager> research;
    std::unique_ptr<ResearchSelector> selector;

    ResearchTestFixture(uint32_t seed = 42)
    {
        techRegistry.Load(FixturePath("techs.json"));
        luaRuntime = std::make_unique<LuaRuntime>();
        techCostConfig = std::make_unique<TechCostConfig_t>(
            TechCostConfigParser::ParseConfig(actest::FixturePath("tech_cost.lua"),
                                       *luaRuntime));
        techCostCalculator = std::make_unique<TechCostCalculator>(*techCostConfig, *luaRuntime);
        research = std::make_unique<ResearchManager>(techRegistry, *techCostCalculator,
                                                     /*pEffectsProvider*/ nullptr);
        selector = std::make_unique<ResearchSelector>(*research, seed);
    }
};

} // namespace

TEST_CASE("GameCategory_t parses canonical category strings", "[research][selector]")
{
    CHECK(ParseGameCategory("build") == GameCategory_t::Build);
    CHECK(ParseGameCategory("GROW") == GameCategory_t::Grow);
    CHECK(ParseGameCategory("Discover") == GameCategory_t::Discover);
    CHECK(ParseGameCategory("conquer") == GameCategory_t::Conquer);
}

TEST_CASE("ResearchSelector prefers selected categories", "[research][selector]")
{
    ResearchTestFixture fixture;
    fixture.selector->SetCategoryEnabled(GameCategory_t::Build, true);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Grow, false);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Discover, false);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Conquer, false);

    const std::vector<const TechConfig_t*> candidates = fixture.selector->GetCandidateTargets();
    REQUIRE(candidates.size() == 7);
    for (const TechConfig_t* pTech : candidates)
    {
        REQUIRE(pTech != nullptr);
        CHECK(pTech->category == GameCategory_t::Build);
    }
    CHECK(std::any_of(candidates.begin(), candidates.end(),
                      [](const TechConfig_t* pTech) { return pTech->id == "build_tech"; }));
}

TEST_CASE("ResearchSelector falls back when selected categories have no available techs", "[research][selector]")
{
    ResearchTestFixture fixture;
    fixture.research->AddDiscoveredTech("build_tech");
    fixture.research->AddDiscoveredTech("grow_tech");
    fixture.research->AddDiscoveredTech("discover_tech");

    fixture.selector->SetCategoryEnabled(GameCategory_t::Build, false);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Grow, true);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Discover, false);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Conquer, false);

    const std::vector<const TechConfig_t*> candidates = fixture.selector->GetCandidateTargets();
    REQUIRE(candidates.size() == 14);

    auto hasTechId = [&candidates](const TechId& rTechId) {
        return std::any_of(candidates.begin(), candidates.end(),
            [&rTechId](const TechConfig_t* pConfig) { return pConfig && pConfig->id == rTechId; });
    };
    CHECK(hasTechId("conquer_tech"));
    CHECK(hasTechId("advanced_build"));
    CHECK(hasTechId("fusion_power"));
    CHECK(hasTechId("intellectual_integrity"));
    CHECK(hasTechId("industrial_automation"));
    CHECK(hasTechId("planetary_economics"));
    CHECK(hasTechId("graviton_theory"));
    CHECK(hasTechId("secrets_of_the_human_brain"));
}

TEST_CASE("ResearchSelector assigns a target from the selected pool", "[research][selector]")
{
    ResearchTestFixture fixture(1234);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Build, false);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Grow, true);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Discover, false);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Conquer, false);

    REQUIRE(fixture.selector->AssignResearchTarget());
    CHECK(fixture.research->HasResearchTarget());
    CHECK(fixture.research->GetResearchTarget() == "grow_tech");
}

TEST_CASE("ResearchSelector ensure always assigns when techs remain", "[research][selector]")
{
    ResearchTestFixture fixture(99);
    fixture.selector->EnsureResearchTarget();
    REQUIRE(fixture.research->HasResearchTarget());

    const TechId firstTarget = fixture.research->GetResearchTarget();
    fixture.research->AddDiscoveredTech(firstTarget);
    fixture.research->ClearResearchTarget();
    CHECK_FALSE(fixture.research->HasResearchTarget());

    fixture.selector->EnsureResearchTarget();
    REQUIRE(fixture.research->HasResearchTarget());
    CHECK(fixture.research->GetResearchTarget() != firstTarget);
}

TEST_CASE("ResearchSelector random assignment stays within selected categories", "[research][selector]")
{
    ResearchTestFixture fixture(7);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Build, true);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Grow, true);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Discover, false);
    fixture.selector->SetCategoryEnabled(GameCategory_t::Conquer, false);

    std::unordered_set<TechId> seen;
    for (int i = 0; i < 20; ++i)
    {
        ResearchSelector selector(*fixture.research, static_cast<uint32_t>(i));
        selector.SetCategoryEnabled(GameCategory_t::Build, true);
        selector.SetCategoryEnabled(GameCategory_t::Grow, true);
        selector.SetCategoryEnabled(GameCategory_t::Discover, false);
        selector.SetCategoryEnabled(GameCategory_t::Conquer, false);
        REQUIRE(selector.AssignResearchTarget());
        seen.insert(fixture.research->GetResearchTarget());
        fixture.research->ClearResearchTarget();
    }

    CHECK(seen.count("build_tech") > 0);
    CHECK(seen.count("grow_tech") > 0);
    CHECK(seen.count("discover_tech") == 0);
    CHECK(seen.count("conquer_tech") == 0);
}

TEST_CASE("ResearchManager: unknown SetResearchTarget leaves prior target intact", "[research]")
{
    ResearchTestFixture fixture;
    fixture.research->SetResearchTarget("build_tech");
    REQUIRE(fixture.research->HasResearchTarget());

    REQUIRE_THROWS_AS(fixture.research->SetResearchTarget("not_a_tech"), std::runtime_error);
    CHECK(fixture.research->HasResearchTarget());
    CHECK(fixture.research->GetResearchTarget() == "build_tech");
}

TEST_CASE("ResearchManager: unknown SetResearchTarget with no prior target stays empty", "[research]")
{
    ResearchTestFixture fixture;
    REQUIRE_THROWS_AS(fixture.research->SetResearchTarget("not_a_tech"), std::runtime_error);
    CHECK_FALSE(fixture.research->HasResearchTarget());
}

TEST_CASE("OnTechDiscovered EnsureResearchTarget fills idle; leaves busy target alone",
          "[research][selector][session]")
{
    actest::FactionFixture fixtures;
    Faction& faction = fixtures.state.AddFaction(std::make_unique<Faction>(
        fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
        fixtures.map, fixtures.settings, fixtures.state.GetUnitIdAllocator(), actest::k_TestFactionSeed));

    ResearchManager& research = faction.GetResearch();
    // Faction construction auto-picks a target; clear to exercise the idle-grant path.
    research.ClearResearchTarget();
    CHECK_FALSE(research.HasResearchTarget());

    // Idle grant: discover assigns a research target via session EnsureResearchTarget.
    research.AddDiscoveredTech("build_tech");
    REQUIRE(research.HasResearchTarget());
    const TechId afterIdle = research.GetResearchTarget();
    CHECK(afterIdle != "build_tech");

    // Busy grant: discovering another tech does not retarget.
    research.AddDiscoveredTech("grow_tech");
    REQUIRE(research.HasResearchTarget());
    CHECK(research.GetResearchTarget() == afterIdle);
}

TEST_CASE("Each base's labs land in base order and a breakthrough drops its surplus",
          "[research]")
{
    actest::FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    BaseManager& rFirst = fixtures.MakeFactionBase(faction, 8, 8);
    BaseManager& rSecond = fixtures.MakeFactionBase(faction, 20, 8);
    rFirst.GetBuildingManager().AddBuilding("Headquarters");
    rFirst.GetBuildingManager().AddBuilding("energy_tap");
    rSecond.GetBuildingManager().AddBuilding("energy_tap");
    faction.ProduceBaseResources();
    const int secondLabs = rSecond.GetLabsProduction();
    REQUIRE(rFirst.GetLabsProduction() > 1);
    REQUIRE(secondLabs > 0);

    ResearchManager& research = faction.GetResearch();
    REQUIRE(research.HasResearchTarget());
    const TechId target = research.GetResearchTarget();
    research.SetAccumulatedPoints(research.GetPointsNeededForCurrentTech() - 1);

    faction.CollectResearch();
    CHECK(research.HasDiscoveredTech(target));
    CHECK(research.GetAccumulatedPoints() == secondLabs);
}

TEST_CASE("A policy is available once its tech is known", "[research][social-engineering]")
{
    actest::FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    SocialPolicyConfig_t policy;
    policy.id = "gated";
    CHECK(PolicyAvailable(faction, policy));

    policy.requiredTech = "build_tech";
    CHECK_FALSE(PolicyAvailable(faction, policy));
    faction.GetResearch().AddDiscoveredTech("build_tech");
    CHECK(PolicyAvailable(faction, policy));
}

TEST_CASE("ForbiddenSocialPolicy blocks PolicyAvailable even with tech",
          "[research][social-engineering]")
{
    actest::FactionFixture fixtures;
    FactionConfig_t definition = fixtures.factionDefinition;
    EffectConfig_t forbid;
    forbid.scope = EffectScope_t::FactionGlobal;
    forbid.effect = ForbiddenSocialPolicyEffect_t{"democratic"};
    definition.effects.push_back(forbid);

    Faction& faction = fixtures.MakeFaction(definition);

    SocialPolicyConfig_t democratic;
    democratic.id = "democratic";
    CHECK_FALSE(PolicyAvailable(faction, democratic));

    SocialPolicyConfig_t frontier;
    frontier.id = "frontier";
    CHECK(PolicyAvailable(faction, frontier));
}

TEST_CASE("Tech cost reads the known-tech count and the tech's own TechCost",
          "[research][cost]")
{
    actest::FactionFixture fixtures;
    Faction& faction = fixtures.MakeFaction();
    ResearchManager& research = faction.GetResearch();
    REQUIRE(research.GetDiscoveredTechs().empty());
    research.AddDiscoveredTech("build_tech");
    research.AddDiscoveredTech("grow_tech");

    TechConfig_t tech;
    tech.id = "priced";
    EffectConfig_t ownCost;
    ownCost.scope = EffectScope_t::ThisTech;
    StatModifierEffect_t modifier;
    modifier.stat = StatId_t::TechCost;
    modifier.amount = 1.25;
    modifier.op = ModifierOp_t::MultiplyGeometric;
    ownCost.effect = modifier;
    tech.effects.push_back(ownCost);

    const TechCostInputs_t inputs = TechCostInputs(research, &faction);
    CHECK(inputs.techs == 2);
    const int formula =
        ResearchFormulaCost(research, &faction, *fixtures.dataContext.techCostCalculator);
    const int scaled = ScaledTechCost(formula, &faction, tech);
    TechConfig_t plain = tech;
    plain.effects.clear();
    const int unscaled = ScaledTechCost(formula, &faction, plain);
    CHECK(scaled == static_cast<int>(std::trunc(static_cast<double>(unscaled) * 1.25)));
}

TEST_CASE("A tech's level is one more than its deepest prerequisite's", "[research][tech-level]")
{
    FactionFixture fixtures;
    const TempConfigFile techs("ac_tech_levels.json", R"([
        { "id": "root", "name": "Root", "category": "build",
          "ai_weights": { "power": 0, "tech": 0, "wealth": 0, "growth": 0 } },
        { "id": "mid", "name": "Mid", "category": "build",
          "ai_weights": { "power": 0, "tech": 0, "wealth": 0, "growth": 0 },
          "prerequisites": ["root"] },
        { "id": "deep", "name": "Deep", "category": "build",
          "ai_weights": { "power": 0, "tech": 0, "wealth": 0, "growth": 0 },
          "prerequisites": ["mid"] },
        { "id": "join", "name": "Join", "category": "build",
          "ai_weights": { "power": 0, "tech": 0, "wealth": 0, "growth": 0 },
          "prerequisites": ["root", "deep"] }
    ])");
    fixtures.dataContext.techRegistry = std::make_unique<TechRegistry>();
    fixtures.dataContext.techRegistry->Load(techs.Path());

    CHECK(TechLevel("root") == 1);
    CHECK(TechLevel("deep") == 3);
    CHECK(TechLevel("join") == 4);

    Faction& rFaction = fixtures.MakeFaction();
    CHECK(HighestTechLevel(rFaction.GetResearch()) == 0);
    rFaction.GetResearch().AddDiscoveredTech("deep");
    rFaction.GetResearch().AddDiscoveredTech("root");
    CHECK(HighestTechLevel(rFaction.GetResearch()) == 3);
}
