#include "TempConfigFile.h"

#include "game/map/ImprovementRegistry.h"
#include <magic_enum.hpp>
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "ui/style/UiStyle.h"
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

TEST_CASE("Pop composition requires formulas and rejects unknown keys",
          "[config][population]")
{
    PopCompositionConfigParser parser;

    SECTION("an empty drone source formula would mean zero drones from that source forever")
    {
        const TempConfigFile config("ac_comp_empty_drone.json", R"({
            "bureaucracy_limit_formula": "1",
            "bureaucracy_drone_formula": "0",
            "size_drone_formula": "",
            "occupation_drone_formula": "0",
            "drone_type": "Drone",
            "talent_type": "Talent"
        })");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path()),
                          Catch::Matchers::ContainsSubstring("size_drone_formula"));
    }

    SECTION("a missing occupation formula is refused rather than defaulted")
    {
        const TempConfigFile config("ac_comp_no_occupation.json", R"({
            "bureaucracy_limit_formula": "1",
            "bureaucracy_drone_formula": "0",
            "size_drone_formula": "0",
            "drone_type": "Drone",
            "talent_type": "Talent"
        })");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path()),
                          Catch::Matchers::ContainsSubstring("occupation_drone_formula"));
    }

    SECTION("an empty bureaucracy limit would make residue modulo undefined")
    {
        const TempConfigFile config("ac_comp_empty_limit.json", R"({
            "bureaucracy_limit_formula": "",
            "bureaucracy_drone_formula": "0",
            "size_drone_formula": "0",
            "occupation_drone_formula": "0",
            "drone_type": "Drone",
            "talent_type": "Talent"
        })");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path()),
                          Catch::Matchers::ContainsSubstring("bureaucracy_limit_formula"));
    }
    SECTION("unknown keys are refused rather than silently ignored")
    {
        const TempConfigFile config("ac_comp_unknown.json", R"({
            "bureaucracy_limit_formula": "1",
            "bureaucracy_drone_formula": "0",
            "size_drone_formula": "0",
            "occupation_drone_formula": "0",
            "drone_type": "Drone",
            "talent_type": "Talent",
            "precedence": ["Talent", "Drone"]
        })");
        CHECK_THROWS_WITH(parser.ParseConfig(config.Path()),
                          Catch::Matchers::ContainsSubstring("unknown key"));
    }

    SECTION("assimilation_drones is required and must be positive")
    {
        const TempConfigFile missing("ac_comp_no_assim_drones.json", R"({
            "bureaucracy_limit_formula": "1",
            "bureaucracy_drone_formula": "0",
            "size_drone_formula": "0",
            "occupation_drone_formula": "0",
            "drone_type": "Drone",
            "talent_type": "Talent",
            "assimilation_decay_turns": 10
        })");
        CHECK_THROWS_WITH(parser.ParseConfig(missing.Path()),
                          Catch::Matchers::ContainsSubstring("assimilation_drones"));

        const TempConfigFile zero("ac_comp_zero_assim_drones.json", R"({
            "bureaucracy_limit_formula": "1",
            "bureaucracy_drone_formula": "0",
            "size_drone_formula": "0",
            "occupation_drone_formula": "0",
            "drone_type": "Drone",
            "talent_type": "Talent",
            "assimilation_drones": 0,
            "assimilation_decay_turns": 10
        })");
        CHECK_THROWS_WITH(parser.ParseConfig(zero.Path()),
                          Catch::Matchers::ContainsSubstring("assimilation_drones"));
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
            { "id": "Worker", "name": "Worker", "display_glyph": "X", "is_default": true,
              "can_work_tile": true },
            { "id": "Specialist", "name": "Specialist", "display_glyph": "X",
              "fallback_pop_type": "Typo" }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("Typo"));
    }

    SECTION("an unknown obsoletes entry is rejected")
    {
        const TempConfigFile config("ac_pop_obsoletes.json", R"([
            { "id": "Worker", "name": "Worker", "display_glyph": "X", "is_default": true,
              "can_work_tile": true },
            { "id": "Doctor", "name": "Doctor", "display_glyph": "X",
              "obsoletes": ["NoSuchType"] }
        ])");
        CHECK_THROWS_WITH(registry.Load(config.Path()),
                          Catch::Matchers::ContainsSubstring("NoSuchType"));
    }

    SECTION("valid references load")
    {
        const TempConfigFile config("ac_pop_ok.json", R"([
            { "id": "Worker", "name": "Worker", "display_glyph": "X", "is_default": true,
              "can_work_tile": true },
            { "id": "Doctor", "name": "Doctor", "display_glyph": "X",
              "fallback_pop_type": "Worker" },
            { "id": "Empath", "name": "Empath", "display_glyph": "X", "obsoletes": ["Doctor"] }
        ])");
        CHECK_NOTHROW(registry.Load(config.Path()));
    }
}

TEST_CASE("A negative improvement energy cost is rejected, by name", "[config][improvement]")
{
    // A terraform order spends this through EconomyManager::CanAfford, which treats a negative
    // cost as a caller bug and throws. Before the treasury owned that rule, a negative cost was
    // an improvement that paid the player to build it. Either way the config is wrong, and it
    // should fail at load naming the improvement rather than mid-order.
    TempConfigFile config("ac_improvement_negative_cost.json", R"([
        { "id": "cheap_farm", "name": "Cheap Farm", "turns_required": 2, "energy_cost": -5 }
    ])");

    ImprovementRegistry registry;
    CHECK_THROWS_WITH(registry.Load(config.Path()),
                      Catch::Matchers::ContainsSubstring("cheap_farm")
                          && Catch::Matchers::ContainsSubstring("energy_cost"));
}

TEST_CASE("An improvement's vision radius comes from its own Vision modifiers",
          "[config][improvement]")
{
    // Resolved once at load; the visibility rebuild reads the field rather than re-resolving
    // the effect per tile per rebuild.
    TempConfigFile config("ac_improvement_vision.json", R"([
        { "id": "watchtower", "name": "Watchtower", "effects": [
            { "type": "StatModifier", "scope": "ThisTile",
              "parameters": { "stat": "vision", "amount": 3, "op": "Add" } }
        ]},
        { "id": "plain_farm", "name": "Plain Farm", "effects": [
            { "type": "StatModifier", "scope": "AllOwnerBases",
              "parameters": { "stat": "nutrients", "amount": 1, "op": "Add" } }
        ]},
        { "id": "aura_only", "name": "Aura Only", "effects": [
            { "type": "StatModifier", "scope": "FactionGlobal",
              "parameters": { "stat": "vision", "amount": 4, "op": "Add" } }
        ]}
    ])");

    ImprovementRegistry registry;
    registry.Load(config.Path());

    CHECK(registry.Get("watchtower").visionRadius == 3);
    CHECK(registry.Get("plain_farm").visionRadius == 0);
    // Sight comes from ThisTile only: a faction-wide Vision modifier is a different axis and
    // must not turn every copy of the improvement into a watchtower.
    CHECK(registry.Get("aura_only").visionRadius == 0);
}

TEST_CASE("An unknown unit-slot column is rejected, by name", "[config][units]")
{
    // "column" was a free string, and everything that was not "right" became left — so a typo
    // silently moved a slot to the other side of the designer with no diagnostic.
    TempConfigFile config("ac_slot_bad_column.json", R"([
        { "id": "reactor", "display_name": "Reactor", "component_type": "reactor",
          "column": "middle" }
    ])");

    UnitSlotRegistry registry;
    CHECK_THROWS_WITH(registry.Load(config.Path()),
                      Catch::Matchers::ContainsSubstring("reactor")
                          && Catch::Matchers::ContainsSubstring("middle"));
}

TEST_CASE("Unit-slot columns accept the shipped wire form", "[config][units]")
{
    TempConfigFile config("ac_slot_columns.json", R"([
        { "id": "chassis", "display_name": "Chassis", "component_type": "chassis",
          "column": "left" },
        { "id": "reactor", "display_name": "Reactor", "component_type": "reactor",
          "column": "right" },
        { "id": "defaulted", "display_name": "Defaulted", "component_type": "ability" }
    ])");

    UnitSlotRegistry registry;
    registry.Load(config.Path());
    CHECK(registry.Get("chassis").column == SlotColumn_t::Left);
    CHECK(registry.Get("reactor").column == SlotColumn_t::Right);
    // Absent means left, as it always did.
    CHECK(registry.Get("defaulted").column == SlotColumn_t::Left);
}

TEST_CASE("A colour with too many components is a typo, not extra data", "[config][ui]")
{
    // ParseColor_ read arr[0..3] and ignored anything past it, so a five-entry array — the shape
    // a mis-edited style file produces — loaded silently with the extra dropped. Driven from the
    // shipped style so the file is otherwise complete and the throw can only come from the
    // colour it mutates.
    std::ifstream in(std::string(AC_CONFIG_DIR) + "/ui/style.json");
    REQUIRE(in.good());
    std::string style((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    const std::string key = "\"background_color\"";
    const size_t at = style.find(key);
    REQUIRE(at != std::string::npos);
    const size_t open = style.find('[', at);
    const size_t close = style.find(']', open);
    REQUIRE(close != std::string::npos);
    style.replace(open, close - open + 1, "[10, 20, 30, 40, 50]");

    TempConfigFile config("ac_style_long_colour.json", style);
    CHECK_THROWS_WITH(ac::UiStyle::Load(config.Path()),
                      Catch::Matchers::ContainsSubstring("background_color"));
}

TEST_CASE("The shipped style file loads", "[config][ui]")
{
    // The counterpart: the mutation above is what breaks it, not the harness.
    CHECK_NOTHROW(ac::UiStyle::Load(std::string(AC_CONFIG_DIR) + "/ui/style.json"));
}

// The shipped configs, loaded through the same registries the game uses. The validators added
// by the review packages (improvement energy_cost >= 0, unit-slot column, exactly one
// "default": true social policy per category) all live at load time, and every test for them
// used a synthetic fixture — so a bad edit to a real config file crashed at faction
// construction with the whole suite green.
TEST_CASE("The shipped improvement config loads", "[config][shipped]")
{
    ImprovementRegistry registry;
    CHECK_NOTHROW(registry.Load(std::string(AC_CONFIG_DIR) + "/improvements.json"));
}

TEST_CASE("The shipped unit-slot config loads", "[config][shipped]")
{
    UnitSlotRegistry registry;
    CHECK_NOTHROW(registry.Load(std::string(AC_CONFIG_DIR) + "/unit_slot_config.json"));
}

TEST_CASE("The shipped social policies declare exactly one default per category",
          "[config][shipped]")
{
    SocialPolicyRegistry registry;
    REQUIRE_NOTHROW(registry.Load(std::string(AC_CONFIG_DIR) + "/social_policies.json"));

    // GetDefaultForCategory is where the rule is enforced; nothing calls it during Load, so a
    // missing or duplicated default reaches a faction constructor instead.
    for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
    {
        CHECK_NOTHROW(registry.GetDefaultForCategory(category));
    }
}

TEST_CASE("The shipped tech and building configs load", "[config][shipped]")
{
    TechRegistry techs;
    CHECK_NOTHROW(techs.Load(std::string(AC_CONFIG_DIR) + "/techs.json"));

    BuildingRegistry buildings;
    CHECK_NOTHROW(buildings.Load(std::string(AC_CONFIG_DIR) + "/buildings"));
}
