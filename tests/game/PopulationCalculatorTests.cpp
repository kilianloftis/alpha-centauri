// Direct tests for the three calculators that had none: RiotCalculator, GoldenAgeCalculator and
// PopTypeAvailabilityCalculator, plus the two rules that moved into config (pop roles and the
// obsolescence graph).

#include "GameFixtures.h"
#include "TempConfigFile.h"
#include "TestHelpers.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/population/calculators/GoldenAgeCalculator.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/calculators/DroneCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "lib/LuaRuntime.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/calculators/RiotCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "lib/Signal.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

using actest::TempConfigFile;

using namespace ac;

namespace
{

// The three signals a RiotCalculator drives, with counters, so transitions can be asserted.
struct RiotSignals_
{
    Signal<> willRiot;
    Signal<> isRioting;
    Signal<> riotEnded;

    int willRiotCount = 0;
    int isRiotingCount = 0;
    int riotEndedCount = 0;

    Signal<>::ScopedConnection willConn;
    Signal<>::ScopedConnection isConn;
    Signal<>::ScopedConnection endedConn;

    RiotSignals_()
        : willConn(willRiot.ConnectScoped([this]() { ++willRiotCount; }))
        , isConn(isRioting.ConnectScoped([this]() { ++isRiotingCount; }))
        , endedConn(riotEnded.ConnectScoped([this]() { ++riotEndedCount; }))
    {
    }
};

RiotConditionInputs_t Calm_()
{
    RiotConditionInputs_t inputs;
    inputs.droneCount = 0;
    inputs.talentCount = 2;
    return inputs;
}

RiotConditionInputs_t Unrest_()
{
    RiotConditionInputs_t inputs;
    inputs.droneCount = 3;
    inputs.talentCount = 1;
    return inputs;
}

} // namespace

TEST_CASE("An incited riot survives the end of turn that follows it", "[population][riot]")
{
    // The Population stage calls Update every turn, and the incited base need not be
    // drone-majority, so only the forced-riot counter can keep this alive.
    RiotSignals_ signals;
    RiotCalculator riot(signals.willRiot, signals.isRioting, signals.riotEnded);

    REQUIRE_FALSE(riot.IsRioting());
    riot.ForceRiot(/*turns=*/1);
    CHECK(riot.IsRioting());
    CHECK(signals.isRiotingCount == 1);

    // The end of turn it was incited on: still rioting, even though nothing about the
    // population composition supports it.
    riot.Update(Calm_());
    CHECK(riot.IsRioting());

    // ...and it expires on the next one.
    riot.Update(Calm_());
    CHECK_FALSE(riot.IsRioting());
    CHECK(signals.riotEndedCount == 1);
}

TEST_CASE("A forced riot lasts its configured number of turns", "[population][riot]")
{
    RiotSignals_ signals;
    RiotCalculator riot(signals.willRiot, signals.isRioting, signals.riotEnded);

    riot.ForceRiot(/*turns=*/3);
    for (int turn = 0; turn < 3; ++turn)
    {
        riot.Update(Calm_());
        CHECK(riot.IsRioting());
    }
    riot.Update(Calm_());
    CHECK_FALSE(riot.IsRioting());
}

TEST_CASE("Forcing a riot extends but never shortens one already running", "[population][riot]")
{
    RiotSignals_ signals;
    RiotCalculator riot(signals.willRiot, signals.isRioting, signals.riotEnded);

    riot.ForceRiot(/*turns=*/5);
    riot.ForceRiot(/*turns=*/1);
    for (int turn = 0; turn < 5; ++turn)
    {
        riot.Update(Calm_());
        CHECK(riot.IsRioting());
    }
    riot.Update(Calm_());
    CHECK_FALSE(riot.IsRioting());
}

TEST_CASE("The natural riot condition still drives itself", "[population][riot]")
{
    RiotSignals_ signals;
    RiotCalculator riot(signals.willRiot, signals.isRioting, signals.riotEnded);

    riot.Update(Unrest_());
    CHECK(riot.IsRioting());

    riot.Update(Calm_());
    CHECK_FALSE(riot.IsRioting());
    CHECK(signals.riotEndedCount == 1);

    // A natural riot outlives an expired forced one.
    riot.ForceRiot(/*turns=*/1);
    riot.Update(Unrest_());
    riot.Update(Unrest_());
    CHECK(riot.IsRioting());
}

TEST_CASE("NotifyPopGrown warns only when a riot is not already running", "[population][riot]")
{
    RiotSignals_ signals;
    RiotCalculator riot(signals.willRiot, signals.isRioting, signals.riotEnded);

    riot.NotifyPopGrown(Calm_());
    CHECK(signals.willRiotCount == 0);

    riot.NotifyPopGrown(Unrest_());
    CHECK(signals.willRiotCount == 1);

    riot.Update(Unrest_());
    riot.NotifyPopGrown(Unrest_());
    CHECK(signals.willRiotCount == 1);
}

TEST_CASE("The composition talent target overrides the actual talent count", "[population][riot]")
{
    RiotSignals_ signals;
    RiotCalculator riot(signals.willRiot, signals.isRioting, signals.riotEnded);

    RiotConditionInputs_t inputs;
    inputs.droneCount = 2;
    inputs.talentCount = 0;
    inputs.targetTalents = 3; // target satisfied: no riot despite zero actual talents

    riot.Update(inputs);
    CHECK_FALSE(riot.IsRioting());

    inputs.targetTalents = 1;
    riot.Update(inputs);
    CHECK(riot.IsRioting());
}

TEST_CASE("GoldenAgeCalculator starts and ends on its condition", "[population][golden-age]")
{
    Signal<> started;
    Signal<> ended;
    int startedCount = 0;
    int endedCount = 0;
    const Signal<>::ScopedConnection startedConn =
        started.ConnectScoped([&]() { ++startedCount; });
    const Signal<>::ScopedConnection endedConn = ended.ConnectScoped([&]() { ++endedCount; });

    GoldenAgeCalculator goldenAge(started, ended);
    CHECK_FALSE(goldenAge.IsInGoldenAge());

    GoldenAgeCalculator::Inputs_t inputs;
    inputs.talentCount = 4;
    inputs.workerCount = 2;
    inputs.specialistCount = 2;
    inputs.droneCount = 0;

    goldenAge.Update(inputs);
    CHECK(goldenAge.IsInGoldenAge());
    CHECK(startedCount == 1);

    // Edge-triggered: holding the condition does not re-emit.
    goldenAge.Update(inputs);
    CHECK(startedCount == 1);

    // A single drone ends it outright, regardless of talent count.
    inputs.droneCount = 1;
    goldenAge.Update(inputs);
    CHECK_FALSE(goldenAge.IsInGoldenAge());
    CHECK(endedCount == 1);

    // Re-enter, so the talent branch below is tested from inside a golden age rather than
    // from a state that is already false.
    inputs.droneCount = 0;
    goldenAge.Update(inputs);
    REQUIRE(goldenAge.IsInGoldenAge());
    CHECK(startedCount == 2);

    // Too few talents ends it: the condition is talents >= workers + specialists.
    inputs.talentCount = 3;
    goldenAge.Update(inputs);
    CHECK_FALSE(goldenAge.IsInGoldenAge());
    CHECK(endedCount == 2);
}

TEST_CASE("Obsolescence is transitive and does not depend on the middle tech",
          "[population][pop-types]")
{
    // A tech order that reaches a late successor without the middle one must still supersede
    // the root: obsolescence closes transitively over the whole graph, not one edge at a time.
    PopTypeRegistry registry;
    registry.Load(actest::FixturePath("pop_types_obsolescence.json"));
    const PopTypeAvailabilityCalculator availability(registry);

    const auto assignableIds = [&](const std::vector<std::string>& rTechs) {
        std::vector<std::string> ids;
        for (const PopTypeConfig_t* pConfig : availability.GetAvailable(rTechs))
        {
            ids.push_back(pConfig->id);
        }
        return ids;
    };
    const auto contains = [](const std::vector<std::string>& rIds, const std::string& rId) {
        return std::find(rIds.begin(), rIds.end(), rId) != rIds.end();
    };

    SECTION("no techs: only the base types")
    {
        const std::vector<std::string> ids = assignableIds({});
        CHECK(contains(ids, "Technician"));
        CHECK_FALSE(contains(ids, "Engineer"));
        CHECK(availability.ResolveCurrentType("Technician", {}).id == "Technician");
    }

    SECTION("the direct successor supersedes it")
    {
        const std::vector<std::string> techs = {"advanced_build"};
        const std::vector<std::string> ids = assignableIds(techs);
        CHECK_FALSE(contains(ids, "Technician"));
        CHECK(contains(ids, "Engineer"));
        CHECK(availability.ResolveCurrentType("Technician", techs).id == "Engineer");
    }

    SECTION("a skipped middle tech still supersedes the root")
    {
        // Transcend obsoletes Engineer, Engineer obsoletes Technician. With Transcend's tech
        // but not Engineer's, Technician must still be superseded - by Transcend.
        const std::vector<std::string> techs = {"transcendence"};
        const std::vector<std::string> ids = assignableIds(techs);
        CHECK_FALSE(contains(ids, "Technician"));
        CHECK_FALSE(contains(ids, "Engineer"));
        CHECK(contains(ids, "Transcend"));
        CHECK(availability.ResolveCurrentType("Technician", techs).id == "Transcend");
        CHECK(availability.ResolveCurrentType("Engineer", techs).id == "Transcend");
    }

    SECTION("GetAvailable and ResolveCurrentType cannot disagree")
    {
        // Both directions: a type is assignable exactly when it is player-assignable, its tech
        // is discovered, and nothing available supersedes it. Asserting only the forward
        // direction would pass for a GetAvailable that returned nothing at all.
        for (const std::vector<std::string>& rTechs :
             {std::vector<std::string>{}, std::vector<std::string>{"advanced_build"},
              std::vector<std::string>{"transcendence"},
              std::vector<std::string>{"advanced_build", "transcendence"}})
        {
            const std::vector<std::string> ids = assignableIds(rTechs);
            int assignableCount = 0;
            for (const PopTypeConfig_t& rConfig : registry.GetAll())
            {
                const bool bResolvesToSelf =
                    availability.ResolveCurrentType(rConfig.id, rTechs).id == rConfig.id;
                const bool bHasTech =
                    rConfig.requiredTech.empty()
                    || std::find(rTechs.begin(), rTechs.end(), rConfig.requiredTech)
                           != rTechs.end();
                const bool bExpected = rConfig.bPlayerAssignable && bHasTech && bResolvesToSelf;

                CHECK(contains(ids, rConfig.id) == bExpected);
                assignableCount += bExpected ? 1 : 0;
            }
            // Guards against both lists being empty and agreeing vacuously.
            CHECK(assignableCount > 0);
            CHECK(ids.size() == static_cast<size_t>(assignableCount));
        }
    }
}

TEST_CASE("An obsolescence cycle is rejected at load", "[population][pop-types][config]")
{
    // The calculator walks this graph for every pop of every base every turn; a cycle there
    // must not be reachable, and a hang is not something a caller's try/catch can absorb.
    PopTypeRegistry registry;
    const TempConfigFile config("ac_pop_cycle.json", R"([
        { "id": "Worker", "name": "Worker", "role": "worker", "display_glyph": "X", "is_default": true,
          "can_work_tile": true },
        { "id": "A", "name": "A", "role": "specialist", "display_glyph": "X", "obsoletes": ["B"] },
        { "id": "B", "name": "B", "role": "specialist", "display_glyph": "X", "obsoletes": ["A"] }
    ])");

    CHECK_THROWS_WITH(registry.Load(config.Path()),
                      Catch::Matchers::ContainsSubstring("cycle"));
}

TEST_CASE("A pop type whose role contradicts can_work_tile is rejected",
          "[population][pop-types][config]")
{
    PopTypeRegistry registry;
    const TempConfigFile config("ac_pop_role_clash.json", R"([
        { "id": "Worker", "name": "Worker", "role": "worker", "display_glyph": "X", "is_default": true,
          "can_work_tile": true },
        { "id": "Odd", "name": "Odd", "role": "specialist", "display_glyph": "X", "can_work_tile": true }
    ])");

    CHECK_THROWS_WITH(registry.Load(config.Path()),
                      Catch::Matchers::ContainsSubstring("Odd")
                          && Catch::Matchers::ContainsSubstring("can_work_tile"));
}

TEST_CASE("A negative drone target is a config error", "[population][composition]")
{
    LuaRuntime lua;

    PopCompositionConfig_t config;
    config.bureaucracyLimitFormula = "1";
    config.droneFormula = "base_size - 10";
    config.droneTypeId = "Drone";
    DroneCalculator droneCalculator(config, lua);

    DroneInputs_t inputs;
    inputs.baseSize = 3;
    CHECK_THROWS_WITH(droneCalculator.Calculate(inputs),
                      Catch::Matchers::ContainsSubstring("Drone")
                          && Catch::Matchers::ContainsSubstring("-7"));

    inputs.baseSize = 12;
    CHECK(droneCalculator.Calculate(inputs) == 2);
}

TEST_CASE("A negative talent target is a config error", "[population][composition]")
{
    LuaRuntime lua;

    PopCompositionConfig_t config;
    config.talentFormula = "psych_output - 10";
    config.talentTypeId = "Talent";

    PopCompositionCalculator calculator(config, lua);

    PopCompositionInputs_t inputs;
    inputs.targetDrones = 0;
    inputs.psychOutput = 3;
    CHECK_THROWS_WITH(calculator.Calculate(inputs),
                      Catch::Matchers::ContainsSubstring("talent")
                          && Catch::Matchers::ContainsSubstring("-7"));

    inputs.psychOutput = 12;
    CHECK(calculator.Calculate(inputs).targetTalents == 2);
}

TEST_CASE("Growth threshold saturates instead of overflowing", "[population][growth]")
{
    // baseSize * nutrientsPerPop was multiplied as int before the rate was applied, so a large
    // modded base size wrapped before the division could bring it back into range.
    const BaseEffects_t noEffects{};

    GrowthConfig_t config;
    config.nutrientsPerPop = 1000000;
    config.maxBaseSize = std::numeric_limits<int>::max();

    CHECK(GrowthCalculator::ComputeNutrientsRequired(config, 1000000, noEffects)
          == std::numeric_limits<int>::max());

    GrowthConfig_t normal;
    normal.nutrientsPerPop = 10;
    normal.maxBaseSize = 8;
    CHECK(GrowthCalculator::ComputeNutrientsRequired(normal, 3, noEffects) == 30);
}

namespace
{

PopCompositionConfig_t BureaucracyConfig_()
{
    PopCompositionConfig_t config;
    config.bureaucracyLimitFormula =
        "math.floor(bureaucracy * math.sqrt(map_width * map_height) / math.sqrt(12800) + 0.5)";
    config.droneFormula =
        "max(0, min(base_size, max(0, floor((residue + faction_base_count - bureaucracy_limit) / bureaucracy_limit)) + max(0, resolved_drones - size_free_drones) + ((assimilation_peak > 0 and assimilation_duration > 0 and turns_since_conquered < assimilation_duration) and 1 or 0) * min(max(0, assimilation_peak - floor(turns_since_conquered / max(1, floor(assimilation_duration / max(1, assimilation_peak))))), max(0, floor(base_size / 4 + conquered_drone_cap)))))";
    config.droneTypeId = "Drone";
    config.talentFormula = "0";
    config.talentTypeId = "Talent";
    config.assimilationDrones = 5;
    config.assimilationDecayTurns = 10;
    return config;
}

DroneInputs_t StandardMapInputs_()
{
    DroneInputs_t inputs;
    inputs.bureaucracy = 32; // Citizen × Efficiency 0 → 8 * 4
    inputs.mapWidth = 80;
    inputs.mapHeight = 40;
    inputs.baseSize = 8;
    // High enough that size drones do not interfere with bureaucracy-only cases.
    inputs.sizeFreeDrones = 100;
    inputs.factionBaseCount = 16;
    return inputs;
}

} // namespace

TEST_CASE("Bureaucracy limit uses resolved bureaucracy and map root",
          "[population][drones][bureaucracy]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    const DroneInputs_t citizen = StandardMapInputs_();
    CHECK(calculator.CalculateLimit(citizen) == 16);

    // Efficiency ≤ 0 still emits MultiplyGeometric 4 → same factor as citizen.
    DroneInputs_t negEffSameAsCitizen = citizen;
    CHECK(calculator.CalculateLimit(negEffSameAsCitizen) == 16);

    DroneInputs_t planned = citizen;
    planned.bureaucracy = 48; // 8 * 6
    CHECK(calculator.CalculateLimit(planned) == 24);

    DroneInputs_t thinker = citizen;
    thinker.bureaucracy = 16; // 4 * 4
    CHECK(calculator.CalculateLimit(thinker) == 8);

    DroneInputs_t transcend = citizen;
    transcend.bureaucracy = 12; // 3 * 4
    CHECK(calculator.CalculateLimit(transcend) == 6);
}

TEST_CASE("Bureaucracy limit rounds to nearest after the full product",
          "[population][drones][bureaucracy]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    DroneInputs_t inputs;
    inputs.bureaucracy = 12; // Transcend × Efficiency 0
    inputs.sizeFreeDrones = 100;

    inputs.mapWidth = 110;
    inputs.mapHeight = 70;
    CHECK(calculator.CalculateLimit(inputs) == 9);

    inputs.mapWidth = 70;
    inputs.mapHeight = 45;
    CHECK(calculator.CalculateLimit(inputs) == 6);

    inputs.bureaucracy = 20; // Librarian × Efficiency 0
    inputs.mapWidth = 110;
    inputs.mapHeight = 70;
    CHECK(calculator.CalculateLimit(inputs) == 16);

    inputs.mapWidth = 70;
    inputs.mapHeight = 45;
    CHECK(calculator.CalculateLimit(inputs) == 10);
}

TEST_CASE("Bureaucracy drones appear only past the limit and follow residue classes",
          "[population][drones][bureaucracy]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);
    const DroneInputs_t atLimit = StandardMapInputs_();
    CHECK(calculator.Calculate(atLimit) == 0);

    DroneInputs_t doubleLimit = atLimit;
    doubleLimit.factionBaseCount = 32;
    CHECK(calculator.Calculate(doubleLimit) == 1);

    DroneInputs_t over = atLimit;
    over.baseId = 42;
    over.factionBaseCount = 17;
    const int residue = static_cast<int>(StableBaseHash(42) % 16);
    CHECK(calculator.Calculate(over) == (residue + 1) / 16);

    DroneInputs_t crowded = atLimit;
    crowded.factionBaseCount = 144;
    crowded.baseSize = 3;
    CHECK(calculator.Calculate(crowded) == 3);
}

TEST_CASE("Bureaucracy limit preserves fractional bureaucracy through the product",
          "[population][drones][bureaucracy]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    DroneInputs_t inputs = StandardMapInputs_();
    inputs.bureaucracy = 32.0 * 0.5; // MultiplyGeometric 0.5 on Citizen×Efficiency
    CHECK(calculator.CalculateLimit(inputs) == 8);
}

TEST_CASE("Size drones appear for every pop past SizeFreeDrones",
          "[population][drones][size]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    // Under the bureaucracy limit so only size contributes.
    // resolvedDrones is Resolve(Drones, seed=baseSize); with no modifiers that is baseSize.
    DroneInputs_t inputs = StandardMapInputs_();
    inputs.factionBaseCount = 1;
    inputs.sizeFreeDrones = 4; // Talent

    inputs.baseSize = 4;
    inputs.resolvedDrones = 4;
    CHECK(calculator.Calculate(inputs) == 0);

    inputs.baseSize = 5;
    inputs.resolvedDrones = 5;
    CHECK(calculator.Calculate(inputs) == 1);

    inputs.baseSize = 8;
    inputs.resolvedDrones = 8;
    CHECK(calculator.Calculate(inputs) == 4);

    inputs.sizeFreeDrones = 6; // Citizen
    inputs.baseSize = 6;
    inputs.resolvedDrones = 6;
    CHECK(calculator.Calculate(inputs) == 0);
    inputs.baseSize = 7;
    inputs.resolvedDrones = 7;
    CHECK(calculator.Calculate(inputs) == 1);
}

TEST_CASE("Size and bureaucracy drone contributions stack, capped by base size",
          "[population][drones][size][bureaucracy]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    DroneInputs_t inputs = StandardMapInputs_();
    inputs.sizeFreeDrones = 4;
    inputs.baseSize = 6;
    inputs.resolvedDrones = 6; // 2 size drones after free
    inputs.factionBaseCount = 32; // 1 bureaucracy drone at double the limit
    CHECK(calculator.Calculate(inputs) == 3);

    inputs.baseSize = 2;
    inputs.resolvedDrones = 2; // size drones 0; bureaucracy still wants 1 → capped at 2
    CHECK(calculator.Calculate(inputs) == 1);
}

TEST_CASE("University-style Drones MultiplyGeometric scales base size before SizeFreeDrones",
          "[population][drones][size]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        actest::Active(pool.StatMod(StatId_t::Drones, 1.25, ModifierOp_t::MultiplyGeometric)),
    };
    // floor(8 * 1.25) = 10; then max(0, 10 - 4) = 6.
    const int resolved =
        FinalizeResolvedStat(ResolveStatModifiers(effects, 8.0).total);
    REQUIRE(resolved == 10);

    DroneInputs_t inputs = StandardMapInputs_();
    inputs.factionBaseCount = 1; // under bureaucracy limit
    inputs.baseSize = 8;
    inputs.sizeFreeDrones = 4;
    inputs.resolvedDrones = resolved;
    CHECK(calculator.Calculate(inputs) == 6);
}

TEST_CASE("A non-positive bureaucracy limit is a config error", "[population][drones][bureaucracy]")
{
    LuaRuntime lua;
    PopCompositionConfig_t config = BureaucracyConfig_();
    config.bureaucracyLimitFormula = "0";
    DroneCalculator calculator(config, lua);

    CHECK_THROWS_WITH(calculator.Calculate(StandardMapInputs_()),
                      Catch::Matchers::ContainsSubstring("limit")
                          && Catch::Matchers::ContainsSubstring("0"));
}

TEST_CASE("Recently-conquered drones decay one per ten turns and respect the cap",
          "[population][drones][conquest]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    // Under the bureaucracy limit, with SizeFreeDrones covering the whole base, so only
    // the assimilation term contributes.
    DroneInputs_t inputs = StandardMapInputs_();
    inputs.factionBaseCount = 1;
    inputs.sizeFreeDrones = 100;
    inputs.resolvedDrones = 8;
    inputs.baseSize = 8;
    inputs.assimilationPeak = 5;
    inputs.assimilationDuration = 50;
    // Cap (8 + 3 - 2)/4 = 2.25 → floor 2. Talent offset 0.75 plus base_conquest −0.5.
    inputs.conqueredDroneCap = 0.25;

    inputs.turnsSinceConquered = 0;
    CHECK(calculator.Calculate(inputs) == 2); // rate 5, cap 2

    inputs.turnsSinceConquered = 9;
    CHECK(calculator.Calculate(inputs) == 2); // still 5, still capped

    inputs.turnsSinceConquered = 10;
    CHECK(calculator.Calculate(inputs) == 2); // rate 4, still capped

    inputs.turnsSinceConquered = 30;
    CHECK(calculator.Calculate(inputs) == 2); // rate 2, cap 2

    inputs.turnsSinceConquered = 40;
    CHECK(calculator.Calculate(inputs) == 1); // rate 1

    inputs.turnsSinceConquered = 50;
    CHECK(calculator.Calculate(inputs) == 0); // window ended

    inputs.assimilationDuration = 0;
    inputs.assimilationPeak = 0;
    inputs.turnsSinceConquered = 0;
    CHECK(calculator.Calculate(inputs) == 0); // never captured
}

TEST_CASE("The conquered-drone cap is (BaseSize + Difficulty - 2) / 4",
          "[population][drones][conquest]")
{
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    DroneInputs_t inputs = StandardMapInputs_();
    inputs.factionBaseCount = 1;
    inputs.sizeFreeDrones = 100;
    inputs.assimilationPeak = 5;
    inputs.assimilationDuration = 50;
    inputs.turnsSinceConquered = 0;

    // Citizen: 0.25 − 0.5 = −0.25. Size 6 → (6+1−2)/4 = 1.
    inputs.baseSize = 6;
    inputs.resolvedDrones = 6;
    inputs.conqueredDroneCap = -0.25;
    CHECK(calculator.Calculate(inputs) == 1);

    // Talent: 0.75 − 0.5 = 0.25. Size 8 → (8+3−2)/4 = 2.
    inputs.baseSize = 8;
    inputs.resolvedDrones = 8;
    inputs.conqueredDroneCap = 0.25;
    CHECK(calculator.Calculate(inputs) == 2);

    // Transcend: 1.5 − 0.5 = 1.0. Size 4 → (4+6−2)/4 = 2.
    inputs.baseSize = 4;
    inputs.resolvedDrones = 4;
    inputs.conqueredDroneCap = 1.0;
    CHECK(calculator.Calculate(inputs) == 2);

    // Citizen size 3 → (3+1−2)/4 = 0.
    inputs.baseSize = 3;
    inputs.resolvedDrones = 3;
    inputs.conqueredDroneCap = -0.25;
    CHECK(calculator.Calculate(inputs) == 0);
}

TEST_CASE("A reversed assimilation window is one drone for twelve turns",
          "[population][drones][conquest]")
{
    // Recapture after 12 turns: duration becomes 12, peak becomes floor(12/10)=1,
    // and duration/peak = 12 so the single drone lasts the whole reversed window.
    LuaRuntime lua;
    const PopCompositionConfig_t config = BureaucracyConfig_();
    DroneCalculator calculator(config, lua);

    DroneInputs_t inputs = StandardMapInputs_();
    inputs.factionBaseCount = 1;
    inputs.sizeFreeDrones = 100;
    inputs.baseSize = 16;
    inputs.resolvedDrones = 16;
    inputs.conqueredDroneCap = 100.0; // cap does not bind
    inputs.assimilationPeak = 1;
    inputs.assimilationDuration = 12;

    inputs.turnsSinceConquered = 0;
    CHECK(calculator.Calculate(inputs) == 1);
    inputs.turnsSinceConquered = 11;
    CHECK(calculator.Calculate(inputs) == 1);
    inputs.turnsSinceConquered = 12;
    CHECK(calculator.Calculate(inputs) == 0);
}
