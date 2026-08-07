#include "TempConfigFile.h"

#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechCostConfig.h"
#include "game/research/TechRegistry.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

using namespace ac;

using actest::TempConfigFile;

TEST_CASE("A tech tree with a prerequisite cycle fails at load", "[config][tech]")
{
    // Self-reference and unknown ids threw; A->B->A did not. ResearchManager only offers a tech
    // once every prerequisite is discovered, so a cyclic component is unreachable forever - a
    // broken tree that presented as techs quietly missing from the research menu.
    TechRegistry registry;

    SECTION("a two-tech cycle is named")
    {
        const TempConfigFile config("ac_tech_cycle.json", R"([
            { "id": "a", "name": "A", "category": "build", "cost": 10, "prerequisites": ["b"] },
            { "id": "b", "name": "B", "category": "build", "cost": 10, "prerequisites": ["a"] }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("cycle")
                              && Catch::Matchers::ContainsSubstring("a")
                              && Catch::Matchers::ContainsSubstring("b"));
    }

    SECTION("a longer cycle is caught too")
    {
        const TempConfigFile config("ac_tech_cycle3.json", R"([
            { "id": "a", "name": "A", "category": "build", "cost": 10, "prerequisites": ["c"] },
            { "id": "b", "name": "B", "category": "build", "cost": 10, "prerequisites": ["a"] },
            { "id": "c", "name": "C", "category": "build", "cost": 10, "prerequisites": ["b"] }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("cycle"));
    }

    SECTION("a diamond is not a cycle")
    {
        const TempConfigFile config("ac_tech_diamond.json", R"([
            { "id": "root", "name": "Root", "category": "build", "cost": 10 },
            { "id": "left", "name": "Left", "category": "build", "cost": 10,
              "prerequisites": ["root"] },
            { "id": "right", "name": "Right", "category": "build", "cost": 10,
              "prerequisites": ["root"] },
            { "id": "join", "name": "Join", "category": "build", "cost": 10,
              "prerequisites": ["left", "right"] }
        ])");
        CHECK_NOTHROW(registry.Load(config.Path()));
    }

    SECTION("a cycle behind a valid prefix is still caught")
    {
        const TempConfigFile config("ac_tech_cycle_deep.json", R"([
            { "id": "root", "name": "Root", "category": "build", "cost": 10 },
            { "id": "mid", "name": "Mid", "category": "build", "cost": 10,
              "prerequisites": ["root", "loop_a"] },
            { "id": "loop_a", "name": "LoopA", "category": "build", "cost": 10,
              "prerequisites": ["loop_b"] },
            { "id": "loop_b", "name": "LoopB", "category": "build", "cost": 10,
              "prerequisites": ["loop_a"] }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("cycle"));
    }
}

TEST_CASE("A tech without a usable cost is rejected, by name", "[config][tech]")
{
    // cost defaulted to 0, and base_cost is already exposed to the cost formula.
    TechRegistry registry;

    SECTION("missing")
    {
        const TempConfigFile config("ac_tech_no_cost.json",
                                 R"([{ "id": "flight", "name": "Flight", "category": "build" }])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("flight")
                              && Catch::Matchers::ContainsSubstring("cost"));
    }

    SECTION("wrong type")
    {
        const TempConfigFile config(
            "ac_tech_bad_cost.json",
            R"([{ "id": "flight", "name": "Flight", "category": "build", "cost": "cheap" }])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("flight")
                              && Catch::Matchers::ContainsSubstring("cost"));
    }
}

TEST_CASE("A wrong-shaped prerequisites list names the tech", "[config][tech]")
{
    // "prerequisites": "tech_x" read identically to no prerequisites at all.
    TechRegistry registry;
    const TempConfigFile config("ac_tech_bad_prereqs.json", R"([
        { "id": "flight", "name": "Flight", "category": "build", "cost": 10,
          "prerequisites": "industrial_base" }
    ])");
    CHECK_THROWS_WITH(registry.Load(config.Path()),
                      Catch::Matchers::ContainsSubstring("flight")
                          && Catch::Matchers::ContainsSubstring("prerequisites"));
}

TEST_CASE("Tech cost refuses to invent a number", "[config][tech]")
{
    // An empty or broken formula evaluated to 0, which std::max(1, cost) turned into a
    // plausible research cost of 1 - a broken mod looked like a cheap tech.
    LuaRuntime lua;
    TechConfig_t tech;
    tech.id = "test_tech";
    tech.cost = 40;
    const TechCostInputs_t inputs;

    SECTION("an empty formula throws rather than costing 1")
    {
        const TechCostConfig_t config;
        REQUIRE(config.costFormula.empty());
        const TechCostCalculator calculator(config, lua);
        CHECK_THROWS_WITH(calculator.CalculateCost(tech, inputs),
                          Catch::Matchers::ContainsSubstring("empty"));
    }

    SECTION("a formula yielding zero throws rather than costing 1")
    {
        const TechCostConfig_t config{"0"};
        const TechCostCalculator calculator(config, lua);
        CHECK_THROWS_WITH(calculator.CalculateCost(tech, inputs),
                          Catch::Matchers::ContainsSubstring("test_tech"));
    }

    SECTION("a working formula is returned as-is")
    {
        const TechCostConfig_t config{"base_cost * 2"};
        const TechCostCalculator calculator(config, lua);
        CHECK(calculator.CalculateCost(tech, inputs) == 80);
    }
}

TEST_CASE("Growth config requires its keys and positive values", "[config][population]")
{
    // json.value(...) meant a typo'd key loaded the struct default, and nutrients_per_pop of 0
    // makes the growth threshold identically 0 - every base grows every turn.
    GrowthConfigParser parser;

    SECTION("a missing key names the key, not just the file")
    {
        const TempConfigFile config("ac_growth_missing.json", R"({"max_base_size": 8})");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path()),
                          Catch::Matchers::ContainsSubstring("nutrients_per_pop")
                              && Catch::Matchers::ContainsSubstring("required"));
    }

    SECTION("a non-positive value is rejected")
    {
        const TempConfigFile config("ac_growth_zero.json",
                                 R"({"nutrients_per_pop": 0, "max_base_size": 8})");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path()),
                          Catch::Matchers::ContainsSubstring("nutrients_per_pop"));
    }

    SECTION("a fractional value is rejected rather than truncated")
    {
        const TempConfigFile config("ac_growth_float.json",
                                 R"({"nutrients_per_pop": 10.9, "max_base_size": 8})");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path()),
                          Catch::Matchers::ContainsSubstring("integer"));
    }

    SECTION("a complete config loads")
    {
        const TempConfigFile config("ac_growth_ok.json",
                                 R"({"nutrients_per_pop": 10, "max_base_size": 8})");
        const GrowthConfig_t growth = parser.ParseConfig(config.Path());
        CHECK(growth.nutrientsPerPop == 10);
        CHECK(growth.maxBaseSize == 8);
    }
}

TEST_CASE("Pop composition requires formulas and rejects the unimplemented key",
          "[config][population]")
{
    LuaRuntime lua;
    PopCompositionConfigParser parser;

    SECTION("an empty formula would mean zero drones forever")
    {
        const TempConfigFile config("ac_comp_empty.lua", R"(return {
            drone_formula = "", talent_formula = "0",
            drone_type = "Drone", talent_type = "Talent",
        })");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path(), lua),
                          Catch::Matchers::ContainsSubstring("drone_formula"));
    }

    SECTION("precedence is refused rather than silently ignored")
    {
        const TempConfigFile config("ac_comp_precedence.lua", R"(return {
            drone_formula = "0", talent_formula = "0",
            drone_type = "Drone", talent_type = "Talent",
            precedence = { "Talent", "Drone" },
        })");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path(), lua),
                          Catch::Matchers::ContainsSubstring("not implemented"));
    }
}

TEST_CASE("A pop type without a role is rejected", "[config][population]")
{
    // Role used to be inferred from riot_contribution / golden_age_contribution magnitudes, so
    // it could not be stated and could not be wrong. Now it is stated, it must be present.
    PopTypeRegistry registry;

    SECTION("missing")
    {
        const TempConfigFile config("ac_pop_no_role.json", R"([
            { "id": "Worker", "name": "Worker", "is_default": true, "can_work_tile": true }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("Worker")
                              && Catch::Matchers::ContainsSubstring("role"));
    }

    SECTION("unknown value")
    {
        const TempConfigFile config("ac_pop_bad_role.json", R"([
            { "id": "Worker", "name": "Worker", "role": "supervisor", "is_default": true,
              "can_work_tile": true }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("supervisor"));
    }
}

TEST_CASE("Pop types validate their references to other pop types", "[config][population]")
{
    // A typo'd fallback surfaced only when a pop actually converted; a typo'd obsoletes entry
    // never surfaced at all - it was silently inert in ResolveCurrentType.
    PopTypeRegistry registry;

    SECTION("an unknown fallback_pop_type is rejected")
    {
        const TempConfigFile config("ac_pop_fallback.json", R"([
            { "id": "Worker", "name": "Worker", "role": "worker", "display_glyph": "X", "is_default": true,
              "can_work_tile": true },
            { "id": "Specialist", "name": "Specialist", "role": "specialist", "display_glyph": "X",
              "fallback_pop_type": "Typo" }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("Typo"));
    }

    SECTION("an unknown obsoletes entry is rejected")
    {
        const TempConfigFile config("ac_pop_obsoletes.json", R"([
            { "id": "Worker", "name": "Worker", "role": "worker", "display_glyph": "X", "is_default": true,
              "can_work_tile": true },
            { "id": "Doctor", "name": "Doctor", "role": "specialist", "display_glyph": "X",
              "obsoletes": ["NoSuchType"] }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("NoSuchType"));
    }

    SECTION("valid references load")
    {
        const TempConfigFile config("ac_pop_ok.json", R"([
            { "id": "Worker", "name": "Worker", "role": "worker", "display_glyph": "X", "is_default": true,
              "can_work_tile": true },
            { "id": "Doctor", "name": "Doctor", "role": "specialist", "display_glyph": "X",
              "fallback_pop_type": "Worker" },
            { "id": "Empath", "name": "Empath", "role": "specialist", "display_glyph": "X", "obsoletes": ["Doctor"] }
        ])");
        CHECK_NOTHROW(registry.Load(config.Path()));
    }
}
