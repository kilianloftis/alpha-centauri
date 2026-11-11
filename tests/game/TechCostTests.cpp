#include "GameFixtures.h"

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchRules.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechConfigParser.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "lib/LuaRuntime.h"
#include "game/social-engineering/SocialPolicyRegistry.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace ac;
using namespace actest;

namespace
{

int FixtureCost_(const TechCostInputs_t& rInputs)
{
    LuaRuntime lua;
    const TechCostConfig_t config =
        TechCostConfigParser::ParseConfig(FixturePath("tech_cost.lua"), lua);
    const TechCostCalculator calculator(config, lua);
    return calculator.CalculateCost(rInputs);
}

TechCostInputs_t NeutralInputs_()
{
    TechCostInputs_t inputs;
    inputs.techs = 1;
    inputs.mostTechs = 1;
    inputs.diff = 3;
    inputs.catchUpDiff = 2;
    inputs.mapAreaSqRoot = 56;
    return inputs;
}

const TechCostCalculator& FixtureCalculator_(FactionFixture& rFixtures)
{
    return *rFixtures.dataContext.techCostCalculator;
}

} // namespace

TEST_CASE("Empty discoveries become TECHS 1; n discoveries stay n", "[research][cost]")
{
    FactionFixture fixtures;
    Faction& rFaction = fixtures.MakeFaction();
    const TechCostInputs_t empty = TechCostInputs(rFaction.GetResearch(), &rFaction);
    CHECK(empty.techs == 1);
    CHECK(empty.mostTechs == 0);

    rFaction.GetResearch().AddDiscoveredTech("build_tech");
    rFaction.GetResearch().AddDiscoveredTech("grow_tech");
    const TechCostInputs_t two = TechCostInputs(rFaction.GetResearch(), &rFaction);
    CHECK(two.techs == 2);
    CHECK(two.mostTechs == 2);
}

TEST_CASE("MOSTTECHS is the max discovered count across every session faction",
          "[research][cost]")
{
    FactionFixture fixtures;
    Faction& rPlayer = fixtures.MakeFaction();
    FactionConfig_t aiDef;
    aiDef.id = "ai_rival";
    Faction& rAi = fixtures.MakeFaction(aiDef);

    rPlayer.GetResearch().AddDiscoveredTech("build_tech");
    rAi.GetResearch().AddDiscoveredTech("build_tech");
    rAi.GetResearch().AddDiscoveredTech("grow_tech");
    rAi.GetResearch().AddDiscoveredTech("discover_tech");

    const TechCostInputs_t player = TechCostInputs(rPlayer.GetResearch(), &rPlayer);
    CHECK(player.techs == 1);
    CHECK(player.mostTechs == 3);
    CHECK_FALSE(player.bIsAI);

    const TechCostInputs_t ai = TechCostInputs(rAi.GetResearch(), &rAi);
    CHECK(ai.techs == 3);
    CHECK(ai.mostTechs == 3);
    CHECK(ai.bIsAI);
}

TEST_CASE("Human Transcend factor clamps to [12 - TECHS, 12 + TECHS]", "[research][cost]")
{
    TechCostInputs_t inputs = NeutralInputs_();
    inputs.diff = 5;
    inputs.techs = 1;
    inputs.mostTechs = 1;
    // 4*5+8 = 28, clamp to [11, 13] → 13; turn lag 1 → 14; no catch-up; * 1 * 56/56.
    CHECK(FixtureCost_(inputs) == 14);
}

TEST_CASE("Talent and Librarian share the factor band and differ on AI catch-up",
          "[research][cost]")
{
    TechCostInputs_t inputs = NeutralInputs_();
    inputs.bIsAI = true;
    inputs.diff = 3;
    inputs.techs = 10;
    inputs.mostTechs = 16;

    inputs.catchUpDiff = 2;
    CHECK(FixtureCost_(inputs) == 290);

    inputs.catchUpDiff = 3;
    CHECK(FixtureCost_(inputs) == 280);
}

TEST_CASE("Human catch-up always uses divisor 5 and 30 percent plus one", "[research][cost]")
{
    TechCostInputs_t inputs = NeutralInputs_();
    inputs.bIsAI = false;
    inputs.diff = 1;
    inputs.catchUpDiff = 0;
    inputs.techs = 10;
    inputs.mostTechs = 16;
    CHECK(FixtureCost_(inputs) == 200);
}

TEST_CASE("Map scale stays in the formula; turn lag uses the slower band", "[research][cost]")
{
    TechCostInputs_t mapped = NeutralInputs_();
    mapped.mapAreaSqRoot = 90;
    CHECK(FixtureCost_(mapped) == 22);

    TechCostInputs_t lagged = NeutralInputs_();
    lagged.techs = 10;
    lagged.mostTechs = 10;
    lagged.turns = 48;
    CHECK(FixtureCost_(lagged) == 240);
    lagged.bTurnLag = true;
    CHECK(FixtureCost_(lagged) == 260);
}

TEST_CASE("Session inputs include map sqrt, turns, and the turn-lag setting",
          "[research][cost]")
{
    FactionFixture fixtures;
    Faction& rFaction = fixtures.MakeFaction();
    const TechCostInputs_t first = TechCostInputs(rFaction.GetResearch(), &rFaction);
    CHECK(first.turns == 0);
    CHECK_FALSE(first.bTurnLag);
    // 36 x 17 holds 306 tiles (x steps by 2), not 612.
    CHECK(first.mapAreaSqRoot == 17);

    fixtures.state.IncrementMissionYear();
    CHECK(TechCostInputs(rFaction.GetResearch(), &rFaction).turns == 1);
    CHECK(first.research == 0);

    fixtures.dataContext.worldRules =
        WorldRulesConfigParser::ParseConfig(FixturePath("world_rules_stagnation.json"));
    FactionConfig_t laggedDef;
    laggedDef.id = "lagged";
    Faction& rLagged = fixtures.MakeFaction(laggedDef);
    const TechCostInputs_t lagged = TechCostInputs(rLagged.GetResearch(), &rLagged);
    CHECK(lagged.bTurnLag);
}

TEST_CASE("TechCostResearch is a formula index; live Research SE does not change it",
          "[research][cost]")
{
    TechCostInputs_t plusTwo = NeutralInputs_();
    plusTwo.techs = 10;
    plusTwo.mostTechs = 10;
    plusTwo.research = 2;
    TechCostInputs_t plusOne = plusTwo;
    plusOne.research = 1;
    TechCostInputs_t none = plusTwo;
    none.research = 0;
    CHECK(FixtureCost_(plusTwo) < FixtureCost_(plusOne));
    CHECK(FixtureCost_(plusOne) < FixtureCost_(none));

    FactionFixture fixtures;
    Faction& rFaction = fixtures.MakeFaction();
    rFaction.GetSocialEngineering().SetActivePolicy(
        fixtures.socialPolicies().Get("research_policy"));
    const TechCostInputs_t liveSe = TechCostInputs(rFaction.GetResearch(), &rFaction);
    CHECK(liveSe.research == 0);

    FactionConfig_t inherentDef;
    inherentDef.id = "inherent_research";
    EffectConfig_t inherent;
    inherent.scope = EffectScope_t::FactionGlobal;
    inherent.effect = StatModifierEffect_t{StatId_t::TechCostResearch, 2.0, ModifierOp_t::Add};
    inherentDef.effects.push_back(inherent);
    Faction& rInherent = fixtures.MakeFaction(inherentDef);
    const TechCostInputs_t fromEffect =
        TechCostInputs(rInherent.GetResearch(), &rInherent);
    CHECK(fromEffect.research == 2);
}

TEST_CASE("TechCost MultiplyGeometric applies after Lua, including world-rules stagnation",
          "[research][cost]")
{
    FactionFixture fixtures;
    fixtures.dataContext.worldRules =
        WorldRulesConfigParser::ParseConfig(FixturePath("world_rules_stagnation.json"));
    Faction& rFaction = fixtures.MakeFaction();
    TechConfig_t tech;
    tech.id = "priced";
    const TechCostCalculator& rCalculator = FixtureCalculator_(fixtures);

    const TechCostInputs_t inputs = TechCostInputs(rFaction.GetResearch(), &rFaction);
    CHECK(inputs.bTurnLag);
    const int luaCost = rCalculator.CalculateCost(inputs);
    const double lua = static_cast<double>(luaCost);
    const int formula = ResearchFormulaCost(rFaction.GetResearch(), &rFaction, rCalculator);
    CHECK(formula == luaCost);
    const int base = ScaledTechCost(formula, &rFaction, tech);
    CHECK(base == static_cast<int>(std::trunc(lua * 1.5)));

    FactionConfig_t cheaperDef;
    cheaperDef.id = "cheaper";
    EffectConfig_t cheaper;
    cheaper.scope = EffectScope_t::FactionGlobal;
    cheaper.effect =
        StatModifierEffect_t{StatId_t::TechCost, 0.8, ModifierOp_t::MultiplyGeometric};
    cheaperDef.effects.push_back(cheaper);
    Faction& rCheaper = fixtures.MakeFaction(cheaperDef);
    const int discounted = ScaledTechCost(formula, &rCheaper, tech);
    CHECK(discounted == static_cast<int>(std::trunc(static_cast<double>(base) * 0.8)));

    tech.effects.push_back(EffectConfig_t{});
    tech.effects.back().scope = EffectScope_t::ThisTech;
    tech.effects.back().effect =
        StatModifierEffect_t{StatId_t::TechCost, 1.25, ModifierOp_t::MultiplyGeometric};
    const int withThisTech = ScaledTechCost(formula, &rCheaper, tech);
    CHECK(withThisTech
          == static_cast<int>(std::trunc(static_cast<double>(base) * 0.8 * 1.25)));
}
