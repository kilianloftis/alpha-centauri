#include "game/map/ImprovementConfigParser.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using namespace ac;

namespace
{

std::filesystem::path WriteTempJson_(const std::string& name, const std::string& body)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << body;
    return path;
}

} // namespace

TEST_CASE("ImprovementConfigParser: turns_required and energy_cost", "[improvements][parser]")
{
    const auto path = WriteTempJson_("ac_improvement_parser.json", R"([
        {
            "id": "Road",
            "name": "Road",
            "turns_required": 1,
            "energy_cost": 0,
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 1);
    CHECK(configs[0].turnsRequired == 1);
    CHECK(configs[0].energyCost == 0);
    CHECK(configs[0].terraformResult == TerraformResult_t::Place);
    std::filesystem::remove(path);
}

TEST_CASE("ImprovementConfigParser: rejects mineral_cost", "[improvements][parser]")
{
    const auto path = WriteTempJson_("ac_improvement_mineral.json", R"([
        { "id": "Road", "name": "Road", "mineral_cost": 5, "effects": [] }
    ])");

    ImprovementConfigParser parser;
    CHECK_THROWS(parser.ParseConfig(path.string()));
    std::filesystem::remove(path);
}

TEST_CASE("ImprovementConfigParser: terraform.result", "[improvements][parser]")
{
    SECTION("raise_land")
    {
        const auto path = WriteTempJson_("ac_improvement_raise.json", R"([
            {
                "id": "RaiseLand",
                "name": "Raise Land",
                "turns_required": 12,
                "terraform": { "result": "raise_land" },
                "effects": []
            }
        ])");
        ImprovementConfigParser parser;
        const auto configs = parser.ParseConfig(path.string());
        REQUIRE(configs.size() == 1);
        CHECK(configs[0].terraformResult == TerraformResult_t::RaiseLand);
        std::filesystem::remove(path);
    }

    SECTION("unknown result throws")
    {
        const auto path = WriteTempJson_("ac_improvement_bad_tf.json", R"([
            {
                "id": "Bad",
                "name": "Bad",
                "terraform": { "result": "dance" },
                "effects": []
            }
        ])");
        ImprovementConfigParser parser;
        CHECK_THROWS(parser.ParseConfig(path.string()));
        std::filesystem::remove(path);
    }
}

TEST_CASE("ImprovementConfigParser: suppress_yield_sources", "[improvements][parser]")
{
    const auto path = WriteTempJson_("ac_improvement_suppress.json", R"([
        {
            "id": "Forest",
            "name": "Forest",
            "turns_required": 4,
            "suppress_yield_sources": ["Rocky", "Wet"],
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 1);
    REQUIRE(configs[0].suppressYieldSources.size() == 2);
    CHECK(configs[0].suppressYieldSources[0] == "Rocky");
    CHECK(configs[0].suppressYieldSources[1] == "Wet");
    std::filesystem::remove(path);
}

TEST_CASE("ImprovementConfigParser loads fixture improvements.json", "[improvements][parser]")
{
    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(std::string(AC_TEST_FIXTURES_DIR) + "/improvements.json");
    REQUIRE(configs.size() > 10);

    const ImprovementConfig_t* pFarm = nullptr;
    const ImprovementConfig_t* pRaise = nullptr;
    for (const auto& c : configs)
    {
        if (c.id == "Farm")
        {
            pFarm = &c;
        }
        if (c.id == "RaiseLand")
        {
            pRaise = &c;
        }
    }
    REQUIRE(pFarm);
    CHECK(pFarm->turnsRequired == 4);
    REQUIRE(pRaise);
    CHECK(pRaise->terraformResult == TerraformResult_t::RaiseLand);
}
