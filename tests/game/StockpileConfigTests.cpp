// The stockpile parser fails the load rather than substituting a default, so a stockpile
// either says what it means or does not load. These cases exist because most of them used to
// be expressible: a stockpile was a building with a `stockpile` flag, so every building field
// applied to it and had to be rejected one by one.

#include "TestHelpers.h"

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/stockpiles/StockpileConfig.h"
#include "game/stockpiles/StockpileRegistry.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <variant>

using namespace ac;

namespace
{

// System temp, not the fixtures dir: this file is rewritten on every run, and a scratch file
// tracked in the repo shows up as a spurious working-tree change after running the tests.
std::string WriteTempStockpiles_(const std::string& contents)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_stockpile_parser.json";
    std::ofstream out(path);
    out << contents;
    return path.string();
}

} // namespace

TEST_CASE("The stockpile parser rejects rather than silently defaulting", "[stockpile][config]")
{
    StockpileRegistry registry;

    SECTION("a valid stockpile loads")
    {
        CHECK_NOTHROW(registry.Load(WriteTempStockpiles_(R"([
            { "id": "Stockpile_Energy", "name": "Stockpile Energy", "rounding": "down",
              "effects": [{
                "type": "StatModifier", "scope": "ThisBase",
                "parameters": { "stat": "econ", "amount": 0.5,
                                "amount_source": "MineralsConverted" }
              }] }
        ])")));
        const StockpileConfig_t* pStockpile = registry.Find("Stockpile_Energy");
        REQUIRE(pStockpile != nullptr);
        CHECK(pStockpile->NeverCompletes());
        CHECK(pStockpile->GetBaseCost() == 0);
        CHECK(pStockpile->rounding == StockpileRounding_t::Down);
        REQUIRE(pStockpile->effects.size() == 1);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&pStockpile->effects.front().effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->amountSource.has_value());
        CHECK(*pMod->amountSource == StatModifierEffect_t::AmountSource_t::MineralsConverted);
        CHECK(pMod->stat == StatId_t::Econ);
    }

    // The rate alone does not say what a leftover fraction is worth. That is balance, so it
    // is config's to state.
    SECTION("rounding is required")
    {
        CHECK_THROWS_WITH(registry.Load(WriteTempStockpiles_(R"([
            { "id": "no_rounding", "name": "No Rounding",
              "effects": [{
                "type": "StatModifier", "scope": "ThisBase",
                "parameters": { "stat": "econ", "amount": 0.5,
                                "amount_source": "MineralsConverted" }
              }] }
        ])")),
                          Catch::Matchers::ContainsSubstring("no_rounding")
                              && Catch::Matchers::ContainsSubstring("rounding"));
    }

    SECTION("an unknown rounding mode is rejected")
    {
        CHECK_THROWS_WITH(registry.Load(WriteTempStockpiles_(R"([
            { "id": "bad_rounding", "name": "Bad", "rounding": "banker",
              "effects": [{
                "type": "StatModifier", "scope": "ThisBase",
                "parameters": { "stat": "econ", "amount": 0.5,
                                "amount_source": "MineralsConverted" }
              }] }
        ])")),
                          Catch::Matchers::ContainsSubstring("banker"));
    }

    SECTION("a stockpile that converts nothing is rejected")
    {
        CHECK_THROWS_WITH(registry.Load(WriteTempStockpiles_(R"([
            { "id": "bare", "name": "Bare", "rounding": "down", "effects": [] }
        ])")),
                          Catch::Matchers::ContainsSubstring("bare")
                              && Catch::Matchers::ContainsSubstring("MineralsConverted"));
    }

    // Minerals are the input. Converting minerals to minerals is a feedback loop.
    SECTION("MineralsConverted on minerals is rejected")
    {
        CHECK_THROWS_WITH(registry.Load(WriteTempStockpiles_(R"([
            { "id": "loop", "name": "Loop", "rounding": "down",
              "effects": [{
                "type": "StatModifier", "scope": "ThisBase",
                "parameters": { "stat": "minerals", "amount": 1,
                                "amount_source": "MineralsConverted" }
              }] }
        ])")),
                          Catch::Matchers::ContainsSubstring("MineralsConverted"));
    }

    // Energy is legal, but it is not a bank: conversion runs it through inefficiency and the
    // econ/labs/psych sliders, exactly like collected tile energy.
    SECTION("MineralsConverted on energy is allowed")
    {
        CHECK_NOTHROW(registry.Load(WriteTempStockpiles_(R"([
            { "id": "raw_energy", "name": "Raw", "rounding": "down",
              "effects": [{
                "type": "StatModifier", "scope": "ThisBase",
                "parameters": { "stat": "energy", "amount": 0.5,
                                "amount_source": "MineralsConverted" }
              }] }
        ])")));
    }

    // A flat Add would pay out on turns with nothing to convert, which is a stipend, not a
    // conversion. Percentage ops scale the converted amount and stay legal.
    SECTION("a flat Add with no amount_source is rejected")
    {
        CHECK_THROWS_WITH(registry.Load(WriteTempStockpiles_(R"([
            { "id": "stipend", "name": "Stipend", "rounding": "down",
              "effects": [
                { "type": "StatModifier", "scope": "ThisBase",
                  "parameters": { "stat": "econ", "amount": 0.5,
                                  "amount_source": "MineralsConverted" } },
                { "type": "StatModifier", "scope": "ThisBase",
                  "parameters": { "stat": "econ", "amount": 3, "op": "Add" } }
              ] }
        ])")),
                          Catch::Matchers::ContainsSubstring("stipend")
                              && Catch::Matchers::ContainsSubstring("Add"));
    }

    SECTION("required_tech, fallback_priority and percentage modifiers are allowed")
    {
        CHECK_NOTHROW(registry.Load(WriteTempStockpiles_(R"([
            { "id": "gated", "name": "Gated", "rounding": "up",
              "required_tech": "advanced_build", "fallback_priority": 5,
              "effects": [
                { "type": "StatModifier", "scope": "ThisBase",
                  "parameters": { "stat": "labs", "amount": 1,
                                  "amount_source": "MineralsConverted" } },
                { "type": "StatModifier", "scope": "ThisBase",
                  "parameters": { "stat": "labs", "amount": 25, "op": "AddPercent" } }
              ] }
        ])")));
        const StockpileConfig_t& rStockpile = registry.Get("gated");
        CHECK(rStockpile.requiredTech == "advanced_build");
        CHECK(rStockpile.fallbackPriority == 5);
        CHECK(rStockpile.rounding == StockpileRounding_t::Up);
        CHECK(rStockpile.effects.size() == 2);
    }

    // Cost, upkeep, secret_project and the rest are building fields. On the old flag-on-a-
    // building shape each one had to be explicitly rejected; now they simply do not exist.
    SECTION("building fields are unknown keys")
    {
        CHECK_THROWS_WITH(registry.Load(WriteTempStockpiles_(R"([
            { "id": "paid", "name": "Paid", "rounding": "down", "mineral_cost": 10,
              "effects": [{
                "type": "StatModifier", "scope": "ThisBase",
                "parameters": { "stat": "econ", "amount": 0.5,
                                "amount_source": "MineralsConverted" }
              }] }
        ])")),
                          Catch::Matchers::ContainsSubstring("paid")
                              && Catch::Matchers::ContainsSubstring("mineral_cost"));

        CHECK_THROWS_WITH(registry.Load(WriteTempStockpiles_(R"([
            { "id": "project", "name": "Project", "rounding": "down", "secret_project": true,
              "effects": [{
                "type": "StatModifier", "scope": "ThisBase",
                "parameters": { "stat": "econ", "amount": 0.5,
                                "amount_source": "MineralsConverted" }
              }] }
        ])")),
                          Catch::Matchers::ContainsSubstring("secret_project"));
    }
}

TEST_CASE("StockpileRegistry::FindFallback ranks by priority behind the tech gate",
          "[stockpile][config]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(R"([
        { "id": "basic", "name": "Basic", "rounding": "down",
          "effects": [{
            "type": "StatModifier", "scope": "ThisBase",
            "parameters": { "stat": "econ", "amount": 0.5,
                            "amount_source": "MineralsConverted" }
          }] },
        { "id": "advanced", "name": "Advanced", "rounding": "down",
          "required_tech": "advanced_build", "fallback_priority": 10,
          "effects": [{
            "type": "StatModifier", "scope": "ThisBase",
            "parameters": { "stat": "labs", "amount": 1,
                            "amount_source": "MineralsConverted" }
          }] }
    ])"));

    REQUIRE(registry.FindFallback({}) != nullptr);
    CHECK(registry.FindFallback({})->id == "basic");
    CHECK(registry.FindFallback({"advanced_build"})->id == "advanced");
}

TEST_CASE("StockpileRegistry::FindFallback returns nullptr when nothing is available",
          "[stockpile][config]")
{
    StockpileRegistry registry;
    registry.Load(WriteTempStockpiles_(R"([
        { "id": "gated", "name": "Gated", "rounding": "down", "required_tech": "advanced_build",
          "effects": [{
            "type": "StatModifier", "scope": "ThisBase",
            "parameters": { "stat": "econ", "amount": 0.5,
                            "amount_source": "MineralsConverted" }
          }] }
    ])"));

    CHECK(registry.FindFallback({}) == nullptr);
    CHECK(registry.FindFallback({"advanced_build"}) != nullptr);
}
