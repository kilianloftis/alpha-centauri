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

TEST_CASE("ImprovementConfigParser: tags are stored on config", "[improvements][parser]")
{
    const auto path = WriteTempJson_("ac_improvement_tags.json", R"([
        {
            "id": "Rocky",
            "name": "Rocky",
            "tags": ["landform"],
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 1);
    REQUIRE(configs[0].tags.size() == 1);
    CHECK(configs[0].tags[0] == "landform");
    std::filesystem::remove(path);
}

TEST_CASE("ImprovementConfigParser: expands @tag in excludes and suppress_yield_sources",
          "[improvements][parser]")
{
    const auto path = WriteTempJson_("ac_improvement_tag_expand.json", R"([
        { "id": "Flat", "name": "Flat", "tags": ["landform"], "effects": [] },
        { "id": "Rocky", "name": "Rocky", "tags": ["landform"], "effects": [] },
        {
            "id": "Forest",
            "name": "Forest",
            "tags": ["land_terraform"],
            "excludes": ["@landform", "Fungus"],
            "suppress_yield_sources": ["@landform"],
            "effects": []
        },
        {
            "id": "LandmarkA",
            "name": "Landmark A",
            "tags": ["landmark"],
            "excludes": ["@landmark", "Monolith"],
            "effects": []
        },
        {
            "id": "LandmarkB",
            "name": "Landmark B",
            "tags": ["landmark"],
            "excludes": ["@landmark", "Monolith"],
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 5);

    const ImprovementConfig_t* pForest = nullptr;
    const ImprovementConfig_t* pLandmarkA = nullptr;
    for (const auto& c : configs)
    {
        if (c.id == "Forest")
        {
            pForest = &c;
        }
        if (c.id == "LandmarkA")
        {
            pLandmarkA = &c;
        }
    }
    REQUIRE(pForest);
    REQUIRE(pForest->excludes == std::vector<std::string>({"Flat", "Rocky", "Fungus"}));
    REQUIRE(pForest->suppressYieldSources == std::vector<std::string>({"Flat", "Rocky"}));
    REQUIRE(pLandmarkA);
    // Self is omitted when expanding a tag the feature itself belongs to.
    REQUIRE(pLandmarkA->excludes == std::vector<std::string>({"LandmarkB", "Monolith"}));
    std::filesystem::remove(path);
}

TEST_CASE("ImprovementConfigParser: @tag expansion omits self from suppress list",
          "[improvements][parser]")
{
    const auto path = WriteTempJson_("ac_improvement_tag_self.json", R"([
        { "id": "Farm", "name": "Farm", "tags": ["land_terraform"], "effects": [] },
        {
            "id": "ThermalBorehole",
            "name": "Thermal Borehole",
            "tags": ["land_terraform"],
            "suppress_yield_sources": ["@land_terraform"],
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 2);
    REQUIRE(configs[1].suppressYieldSources == std::vector<std::string>({"Farm"}));
    std::filesystem::remove(path);
}

TEST_CASE("ImprovementConfigParser: tag expansion deduplicates mixed ids", "[improvements][parser]")
{
    const auto path = WriteTempJson_("ac_improvement_tag_dedupe.json", R"([
        { "id": "Flat", "name": "Flat", "tags": ["landform"], "effects": [] },
        { "id": "Rocky", "name": "Rocky", "tags": ["landform"], "effects": [] },
        {
            "id": "Forest",
            "name": "Forest",
            "suppress_yield_sources": ["Flat", "@landform", "Rocky"],
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 3);
    REQUIRE(configs[2].suppressYieldSources == std::vector<std::string>({"Flat", "Rocky"}));
    std::filesystem::remove(path);
}

TEST_CASE("ImprovementConfigParser: unknown @tag throws", "[improvements][parser]")
{
    const auto path = WriteTempJson_("ac_improvement_bad_tag.json", R"([
        {
            "id": "Forest",
            "name": "Forest",
            "suppress_yield_sources": ["@missing"],
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    CHECK_THROWS(parser.ParseConfig(path.string()));
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
