#include "GameFixtures.h"

#include "game/DifficultyConfig.h"
#include "game/DifficultyConfigParser.h"
#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameRulesConfig.h"
#include "game/GameSettings.h"
#include "game/buildings/BuildingUpkeep.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/population/calculators/DroneCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/UnitDesign.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <vector>

using namespace ac;
using namespace actest;
using Catch::Approx;

namespace
{

std::string ShippingDifficultyPath()
{
    return std::string(AC_TEST_FIXTURES_DIR) + "/../../config/difficulty.json";
}

void SelectDifficulty_(FactionFixture& rFixtures, const char* difficultyId);

// Swaps in the shipping levels and selects one. Setting game rules bumps the revision the
// effects pool samples, so any faction built earlier re-resolves against the new config.
void UseShippingDifficulty_(FactionFixture& rFixtures, const char* difficultyId)
{
    rFixtures.dataContext.difficultyConfig = std::make_unique<DifficultyConfig_t>(
        DifficultyConfigParser{}.ParseConfig(ShippingDifficultyPath()));
    SelectDifficulty_(rFixtures, difficultyId);
}

void SelectDifficulty_(FactionFixture& rFixtures, const char* difficultyId)
{
    GameRulesConfig_t rules = rFixtures.settings.GetGameRules();
    rules.difficultyId = difficultyId;
    rFixtures.settings.SetGameRules(rules);
}

double ResolveBaseStat_(const BaseManager& rBase, StatId_t stat, double seed)
{
    return ResolveStatModifiers(
               FilterBaseLevelByStatId(rBase.GetBaseEffects(), stat), seed)
        .total;
}

} // namespace

TEST_CASE("DifficultyConfigParser loads all six shipping levels", "[difficulty][parser]")
{
    const DifficultyConfig_t config =
        DifficultyConfigParser{}.ParseConfig(ShippingDifficultyPath());

    CHECK(config.defaultId == "talent");
    REQUIRE(config.levels.size() == 6);
    CHECK(config.RequireForSession("").id == "talent"); // empty defers to "default"
    CHECK(config.RequireForSession("citizen").id == "citizen");
    CHECK(config.RequireForSession("transcend").id == "transcend");
    CHECK(config.RequireForSession("librarian").id == "librarian");
    CHECK_THROWS(config.RequireForSession("nonesuch"));

    const DifficultyLevel_t& rCitizen = config.RequireForSession("citizen");
    CHECK(rCitizen.rules.randomEventsAfterTurn == 75);
    CHECK(rCitizen.rules.researchDisabledTurns == 5);
    CHECK(rCitizen.rules.aiSecretProjectsRequireHumanPrereq);
    CHECK_FALSE(rCitizen.rules.aiAutoPersonality);
    CHECK(rCitizen.rules.combatHandicap);
    CHECK_FALSE(rCitizen.rules.combatHandicapNativesOnly);

    const DifficultyLevel_t& rTalent = config.RequireForSession("talent");
    CHECK(rTalent.rules.combatHandicapNativesOnly);
    CHECK(rTalent.rules.researchDisabledTurns == 0);
}

TEST_CASE("DifficultyConfigParser rejects bad configs", "[difficulty][parser]")
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_bad_difficulty.json";

    {
        std::ofstream out(path);
        out << R"({"default":"missing","levels":[{"id":"citizen"}]})";
    }
    CHECK_THROWS_WITH(DifficultyConfigParser{}.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("default"));

    {
        std::ofstream out(path);
        out << R"({
          "default":"citizen",
          "levels":[
            {"id":"citizen"},
            {"id":"citizen"}
          ]
        })";
    }
    CHECK_THROWS_WITH(DifficultyConfigParser{}.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("duplicate level id"));

    std::filesystem::remove(path);
}

TEST_CASE("AI CostMultiplier applies only to AI factions", "[difficulty][effects]")
{
    FactionFixture fixtures;
    UseShippingDifficulty_(fixtures, "citizen");

    Faction& rHuman = fixtures.MakeFaction();
    Faction& rAi = fixtures.MakeFaction();
    BaseManager& rHumanBase = fixtures.MakeFactionBase(rHuman, 2, 2);
    BaseManager& rAiBase = fixtures.MakeFactionBase(rAi, 5, 5);

    CHECK(ResolveBaseStat_(rHumanBase, StatId_t::CostMultiplier, 1.0) == Approx(1.0));
    CHECK(ResolveBaseStat_(rAiBase, StatId_t::CostMultiplier, 1.0) == Approx(1.3));
}

TEST_CASE("Citizen MaxClamp cancels the last-defender baseline only", "[difficulty][effects]")
{
    FactionFixture fixtures;
    UseShippingDifficulty_(fixtures, "citizen");

    Faction& rFaction = fixtures.MakeFaction();
    BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 3, 3);

    // base_conquest.json Adds the 1-pop baseline; Citizen's MaxClamp 0 cancels it.
    CHECK(ResolveBaseStat_(rBase, StatId_t::LastDefenderPopLoss, 0.0) == Approx(0.0));
    // Difficulty does not touch capture loss — the baseline Add stands.
    CHECK(ResolveBaseStat_(rBase, StatId_t::CapturePopLoss, 0.0) == Approx(1.0));
}

TEST_CASE("SizeFreeDrones follows the difficulty matrix", "[difficulty][effects]")
{
    FactionFixture fixtures;
    UseShippingDifficulty_(fixtures, "talent");

    Faction& rFaction = fixtures.MakeFaction();
    BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 3, 3);

    CHECK(ResolveBaseStat_(rBase, StatId_t::SizeFreeDrones, 0.0) == Approx(4.0));
}

TEST_CASE("ConqueredDroneCap is 0.25 per difficulty level plus base_conquest -0.5",
          "[difficulty][effects][conquest]")
{
    const auto resolveCap = [](const char* difficultyId) {
        FactionFixture fixtures;
        UseShippingDifficulty_(fixtures, difficultyId);
        Faction& rFaction = fixtures.MakeFaction();
        BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 3, 3);
        return ResolveBaseStat_(rBase, StatId_t::ConqueredDroneCap, 0.0);
    };

    // (Difficulty − 2)/4 with Citizen = 1. Fixture base_conquest Adds −0.5.
    CHECK(resolveCap("citizen") == Approx(-0.25));
    CHECK(resolveCap("specialist") == Approx(0.0));
    CHECK(resolveCap("talent") == Approx(0.25));
    CHECK(resolveCap("librarian") == Approx(0.5));
    CHECK(resolveCap("thinker") == Approx(0.75));
    CHECK(resolveCap("transcend") == Approx(1.0));
}

TEST_CASE("EcologicalDamage MultiplyGeometric is present on Talent", "[difficulty][effects]")
{
    FactionFixture fixtures;
    UseShippingDifficulty_(fixtures, "talent");
    Faction& rFaction = fixtures.MakeFaction();
    BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 3, 3);

    CHECK(ResolveBaseStat_(rBase, StatId_t::EcologicalDamage, 1.0) == Approx(3.0));
}

TEST_CASE("TechCostDiff follows difficulty banding", "[difficulty][effects]")
{
    const auto resolveDiff = [](const char* difficultyId) {
        FactionFixture fixtures;
        UseShippingDifficulty_(fixtures, difficultyId);
        Faction& rFaction = fixtures.MakeFaction();
        BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 3, 3);
        return ResolveBaseStat_(rBase, StatId_t::TechCostDiff, 0.0);
    };

    CHECK(resolveDiff("citizen") == Approx(1.0));
    CHECK(resolveDiff("specialist") == Approx(2.0));
    CHECK(resolveDiff("talent") == Approx(3.0));
    CHECK(resolveDiff("librarian") == Approx(3.0));
    CHECK(resolveDiff("thinker") == Approx(4.0));
    CHECK(resolveDiff("transcend") == Approx(5.0));
}

TEST_CASE("Bureaucracy PureMultiplier multiplies difficulty and Efficiency",
          "[difficulty][effects][bureaucracy]")
{
    const auto resolveBureaucracy = [](const char* difficultyId) {
        FactionFixture fixtures;
        UseShippingDifficulty_(fixtures, difficultyId);
        // Shipping Efficiency SE emits MultiplyGeometric on bureaucracy; fixture SE does not.
        fixtures.dataContext.socialRatingRegistry = std::make_unique<SocialRatingRegistry>();
        fixtures.dataContext.socialRatingRegistry->Load(
            std::string(AC_TEST_FIXTURES_DIR) + "/../../config/social_rating_effects.json");
        Faction& rFaction = fixtures.MakeFaction();
        BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 3, 3);
        return ResolveBaseStat_(rBase, StatId_t::Bureaucracy, 1.0);
    };

    // Difficulty geometric × Efficiency 0 geometric (×4).
    CHECK(resolveBureaucracy("citizen") == Approx(32.0));   // 8 * 4
    CHECK(resolveBureaucracy("talent") == Approx(24.0));    // 6 * 4
    CHECK(resolveBureaucracy("librarian") == Approx(20.0)); // 5 * 4
    CHECK(resolveBureaucracy("transcend") == Approx(12.0)); // 3 * 4
}

TEST_CASE("Bureaucracy drones distribute past the limit end-to-end",
          "[difficulty][effects][bureaucracy][drones]")
{
    // Citizen + Efficiency 0 on 80×40 → limit 16.
    FactionFixture fixtures(80, 40);
    UseShippingDifficulty_(fixtures, "citizen");
    fixtures.dataContext.socialRatingRegistry = std::make_unique<SocialRatingRegistry>();
    fixtures.dataContext.socialRatingRegistry->Load(
        std::string(AC_TEST_FIXTURES_DIR) + "/../../config/social_rating_effects.json");
    fixtures.dataContext.popCompositionConfig = std::make_unique<PopCompositionConfig_t>(
        PopCompositionConfigParser{}.ParseConfig(
            std::string(AC_TEST_FIXTURES_DIR) + "/../../config/pop_composition.json"));
    fixtures.dataContext.droneCalculator = std::make_unique<DroneCalculator>(
        *fixtures.dataContext.popCompositionConfig, *fixtures.dataContext.luaRuntime);
    fixtures.dataContext.popCompositionCalculator = std::make_unique<PopCompositionCalculator>(
        *fixtures.dataContext.popCompositionConfig, *fixtures.dataContext.popTypeRegistry);

    Faction& rFaction = fixtures.MakeFaction();
    std::vector<BaseManager*> bases;
    int x = 0;
    int y = 0;
    for (int i = 0; i < 16; ++i)
    {
        bases.push_back(&fixtures.MakeFactionBase(rFaction, x, y));
        x += 2;
        if (x >= 80)
        {
            x = 0;
            y += 2;
        }
    }
    for (BaseManager* pBase : bases)
    {
        pBase->GetPopulation().RecalculateComposition();
        CHECK(pBase->GetPopulation().GetDroneCount() == 0);
    }

    BaseManager& rOver = fixtures.MakeFactionBase(rFaction, x, y);
    bases.push_back(&rOver);
    for (BaseManager* pBase : bases)
    {
        pBase->GetPopulation().RecalculateComposition();
    }

    const int residue = static_cast<int>(StableBaseHash(rOver.GetBaseId()) % 16);
    CHECK(rOver.GetPopulation().GetDroneCount() == (residue + 1) / 16);
}

TEST_CASE("A captured base gains recently-conquered drones",
          "[difficulty][effects][drones][conquest]")
{
    FactionFixture fixtures(80, 40);
    UseShippingDifficulty_(fixtures, "citizen");
    fixtures.dataContext.socialRatingRegistry = std::make_unique<SocialRatingRegistry>();
    fixtures.dataContext.socialRatingRegistry->Load(
        std::string(AC_TEST_FIXTURES_DIR) + "/../../config/social_rating_effects.json");
    fixtures.dataContext.popCompositionConfig = std::make_unique<PopCompositionConfig_t>(
        PopCompositionConfigParser{}.ParseConfig(
            std::string(AC_TEST_FIXTURES_DIR) + "/../../config/pop_composition.json"));
    fixtures.dataContext.droneCalculator = std::make_unique<DroneCalculator>(
        *fixtures.dataContext.popCompositionConfig, *fixtures.dataContext.luaRuntime);
    fixtures.dataContext.popCompositionCalculator = std::make_unique<PopCompositionCalculator>(
        *fixtures.dataContext.popCompositionConfig, *fixtures.dataContext.popTypeRegistry);

    Faction& rGiver = fixtures.MakeFaction();
    Faction& rTaker = fixtures.MakeFaction();
    BaseManager& rBase = fixtures.MakeFactionBase(rGiver, 3, 3);
    while (rBase.GetPopulation().GetSize() < 6)
    {
        rBase.GetPopulation().AddPop();
    }
    rBase.GetPopulation().RecalculateComposition();
    CHECK(rBase.GetPopulation().GetDroneCount() == 0);

    rBase.GetPopulation().NotifyCaptured(rGiver.GetFactionId(), rTaker.GetFactionId());
    rGiver.TransferBaseTo(rBase.GetBaseId(), rTaker);
    // Cap (6 + 1 − 2)/4 = 1 extra drone while assimilating.
    CHECK(rBase.GetPopulation().GetDroneCount() == 1);
    CHECK(rBase.GetPopulation().GetAssimilation().IsAssimilating());
}

TEST_CASE("ProbeActionCost -50 applies to AI bases on Citizen", "[difficulty][effects]")
{
    FactionFixture fixtures;
    UseShippingDifficulty_(fixtures, "citizen");

    Faction& rHuman = fixtures.MakeFaction();
    Faction& rAi = fixtures.MakeFaction();
    BaseManager& rHumanBase = fixtures.MakeFactionBase(rHuman, 2, 2);
    BaseManager& rAiBase = fixtures.MakeFactionBase(rAi, 5, 5);

    CHECK(ResolveBaseStat_(rHumanBase, StatId_t::ProbeActionCost, 1.0) == Approx(1.0));
    CHECK(ResolveBaseStat_(rAiBase, StatId_t::ProbeActionCost, 1.0) == Approx(0.5));
}

TEST_CASE("Command Center MaxClamp follows difficulty", "[difficulty][effects][upkeep]")
{
    FactionFixture fixtures;
    UseShippingDifficulty_(fixtures, "citizen");

    Faction& rFaction = fixtures.MakeFaction();
    BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 3, 3);

    BuildingConfig_t synthetic;
    synthetic.id = "Command_Center";
    synthetic.upkeep = 2;
    CHECK(ResolveFacilityEnergyUpkeepPerCopy(synthetic, rBase.GetBaseEffects().effects) == 0);
}

TEST_CASE("Changing difficulty mid-campaign re-resolves live factions", "[difficulty][effects]")
{
    FactionFixture fixtures;
    UseShippingDifficulty_(fixtures, "talent");

    Faction& rHuman = fixtures.MakeFaction();
    Faction& rAi = fixtures.MakeFaction();
    BaseManager& rHumanBase = fixtures.MakeFactionBase(rHuman, 2, 2);
    BaseManager& rAiBase = fixtures.MakeFactionBase(rAi, 5, 5);

    // Resolve first, so the pools are cached against Talent before the switch.
    CHECK(ResolveBaseStat_(rHumanBase, StatId_t::SizeFreeDrones, 0.0) == Approx(4.0));
    CHECK(ResolveBaseStat_(rAiBase, StatId_t::CostMultiplier, 1.0) == Approx(1.1));

    SelectDifficulty_(fixtures, "transcend");

    // Every faction picks up the new level without being rebuilt.
    CHECK(ResolveBaseStat_(rHumanBase, StatId_t::SizeFreeDrones, 0.0) == Approx(1.0));
    CHECK(ResolveBaseStat_(rAiBase, StatId_t::CostMultiplier, 1.0) == Approx(0.7));

    // And back again — the pool is not one-way latched.
    SelectDifficulty_(fixtures, "citizen");
    CHECK(ResolveBaseStat_(rHumanBase, StatId_t::SizeFreeDrones, 0.0) == Approx(6.0));
    CHECK(ResolveBaseStat_(rAiBase, StatId_t::CostMultiplier, 1.0) == Approx(1.3));
}
