#include "game/faction/ResearchManager.h"
#include "game/faction/ResearchSelector.h"
#include "game/GameCategory.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "lib/LuaRuntime.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <unordered_set>

using namespace ac;
using namespace actest;

namespace
{

struct ResearchTestFixture
{
    TechRegistry techRegistry;
    std::unique_ptr<LuaRuntime> luaRuntime;
    std::unique_ptr<TechCostConfig> techCostConfig;
    std::unique_ptr<TechCostCalculator> techCostCalculator;
    std::unique_ptr<ResearchManager> research;
    std::unique_ptr<ResearchSelector> selector;

    ResearchTestFixture(uint32_t seed = 42)
    {
        techRegistry.Load(FixturePath("techs.json"));
        luaRuntime = std::make_unique<LuaRuntime>();
        TechCostConfigParser techCostParser;
        techCostConfig = std::make_unique<TechCostConfig>(
            techCostParser.ParseConfig(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/tech_cost.lua",
                                       *luaRuntime));
        techCostCalculator = std::make_unique<TechCostCalculator>(*techCostConfig, *luaRuntime);
        research = std::make_unique<ResearchManager>(&techRegistry, techCostCalculator.get(), nullptr);
        selector = std::make_unique<ResearchSelector>(research.get(), seed);
    }
};

} // namespace

TEST_CASE("GameCategory parses canonical category strings", "[research][selector]")
{
    CHECK(GameCategoryToString(GameCategory::Build) == "build");
    CHECK(GameCategoryToString(GameCategory::Grow) == "grow");
    CHECK(GameCategoryToString(GameCategory::Discover) == "discover");
    CHECK(GameCategoryToString(GameCategory::Conquer) == "conquer");

    CHECK(ParseGameCategory("build") == GameCategory::Build);
    CHECK(ParseGameCategory("GROW") == GameCategory::Grow);
    CHECK(ParseGameCategory("Discover") == GameCategory::Discover);
    CHECK(ParseGameCategory("conquer") == GameCategory::Conquer);
}

TEST_CASE("ResearchSelector prefers selected categories", "[research][selector]")
{
    ResearchTestFixture fixture;
    fixture.selector->SetCategoryEnabled(GameCategory::Build, true);
    fixture.selector->SetCategoryEnabled(GameCategory::Grow, false);
    fixture.selector->SetCategoryEnabled(GameCategory::Discover, false);
    fixture.selector->SetCategoryEnabled(GameCategory::Conquer, false);

    const std::vector<const TechConfig_t*> candidates = fixture.selector->GetCandidateTargets();
    REQUIRE(candidates.size() == 1);
    REQUIRE(candidates.front() != nullptr);
    CHECK(candidates.front()->id == "build_tech");
}

TEST_CASE("ResearchSelector falls back when selected categories have no available techs", "[research][selector]")
{
    ResearchTestFixture fixture;
    fixture.research->AddDiscoveredTech("build_tech");
    fixture.research->AddDiscoveredTech("grow_tech");
    fixture.research->AddDiscoveredTech("discover_tech");

    fixture.selector->SetCategoryEnabled(GameCategory::Build, false);
    fixture.selector->SetCategoryEnabled(GameCategory::Grow, true);
    fixture.selector->SetCategoryEnabled(GameCategory::Discover, false);
    fixture.selector->SetCategoryEnabled(GameCategory::Conquer, false);

    const std::vector<const TechConfig_t*> candidates = fixture.selector->GetCandidateTargets();
    REQUIRE(candidates.size() == 2);

    auto hasTechId = [&candidates](const TechId& rTechId) {
        return std::any_of(candidates.begin(), candidates.end(),
            [&rTechId](const TechConfig_t* pConfig) { return pConfig && pConfig->id == rTechId; });
    };
    CHECK(hasTechId("conquer_tech"));
    CHECK(hasTechId("advanced_build"));
}

TEST_CASE("ResearchSelector assigns a target from the selected pool", "[research][selector]")
{
    ResearchTestFixture fixture(1234);
    fixture.selector->SetCategoryEnabled(GameCategory::Build, false);
    fixture.selector->SetCategoryEnabled(GameCategory::Grow, true);
    fixture.selector->SetCategoryEnabled(GameCategory::Discover, false);
    fixture.selector->SetCategoryEnabled(GameCategory::Conquer, false);

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
    fixture.selector->SetCategoryEnabled(GameCategory::Build, true);
    fixture.selector->SetCategoryEnabled(GameCategory::Grow, true);
    fixture.selector->SetCategoryEnabled(GameCategory::Discover, false);
    fixture.selector->SetCategoryEnabled(GameCategory::Conquer, false);

    std::unordered_set<TechId> seen;
    for (int i = 0; i < 20; ++i)
    {
        ResearchSelector selector(fixture.research.get(), static_cast<uint32_t>(i));
        selector.SetCategoryEnabled(GameCategory::Build, true);
        selector.SetCategoryEnabled(GameCategory::Grow, true);
        selector.SetCategoryEnabled(GameCategory::Discover, false);
        selector.SetCategoryEnabled(GameCategory::Conquer, false);
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
