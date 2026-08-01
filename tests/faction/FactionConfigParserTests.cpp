#include "game/faction/FactionConfigParser.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace ac;

namespace
{

std::string FixturesFactionsDir()
{
    return std::string(AC_TEST_FIXTURES_DIR) + "/factions";
}

} // namespace

TEST_CASE("FactionConfigParser loads per-faction directory layout", "[faction][parser]")
{
    FactionConfigParser parser;
    const std::vector<FactionConfig_t> configs = parser.ParseConfig(FixturesFactionsDir());

    REQUIRE(configs.size() == 2);

    const FactionConfig_t* pComplete = nullptr;
    const FactionConfig_t* pMinimal = nullptr;
    for (const FactionConfig_t& rConfig : configs)
    {
        if (rConfig.id == "complete")
        {
            pComplete = &rConfig;
        }
        else if (rConfig.id == "minimal")
        {
            pMinimal = &rConfig;
        }
    }

    REQUIRE(pComplete != nullptr);
    CHECK(pComplete->identity.name == "Test Faction");
    CHECK(pComplete->identity.descriptiveName == "Test Faction Command");
    CHECK(pComplete->identity.noun == "Tester");
    CHECK(pComplete->identity.adjective == "Testers");
    CHECK(pComplete->leader.name == "Test Leader");
    CHECK(pComplete->leader.title == "Chief Tester");
    CHECK(pComplete->ai.wealth);
    CHECK_FALSE(pComplete->ai.power);
    CHECK(pComplete->ai.growth);
    CHECK(pComplete->ai.tech);
    REQUIRE(pComplete->effects.size() == 1);
    CHECK(pComplete->flavor.baseNames == std::vector<std::string>{"Alpha Base", "Beta Base",
                                                                  "Gamma Base"});
    REQUIRE(pComplete->flavor.phrases.count("greeting") == 1);
    CHECK(pComplete->flavor.phrases.at("greeting").size() == 2);

    REQUIRE(pMinimal != nullptr);
    CHECK(pMinimal->identity.name == "Minimal");
    CHECK(pMinimal->identity.descriptiveName == "Minimal");
    CHECK(pMinimal->identity.noun == "Minimal");
    CHECK(pMinimal->identity.adjective == "Minimal");
    CHECK(pMinimal->identity.participatesInCouncil);
    CHECK(pComplete->identity.participatesInCouncil);
    CHECK(pMinimal->leader.name == "Leader");
    CHECK(pMinimal->leader.title.empty());
    CHECK_FALSE(pMinimal->ai.wealth);
    CHECK_FALSE(pMinimal->ai.power);
    CHECK_FALSE(pMinimal->ai.growth);
    CHECK_FALSE(pMinimal->ai.tech);
    CHECK(pMinimal->effects.empty());
    CHECK(pMinimal->flavor.baseNames.empty());
    CHECK(pMinimal->flavor.phrases.empty());
}

TEST_CASE("FactionConfigParser throws when required files are missing", "[faction][parser]")
{
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "ac_faction_parser_missing_test";
    std::filesystem::remove_all(tempDir);
    const std::filesystem::path factionDir = tempDir / "broken";
    std::filesystem::create_directories(factionDir);

    FactionConfigParser parser;

    try
    {
        parser.ParseConfig(tempDir.string());
        FAIL("Expected missing identity.json to throw");
    }
    catch (const std::runtime_error& e)
    {
        CHECK(std::string(e.what()) == "Required faction config file missing: "
                                       + (factionDir / "identity.json").string());
    }

    std::ofstream(factionDir / "identity.json") << R"({"name": "Broken"})";

    try
    {
        parser.ParseConfig(tempDir.string());
        FAIL("Expected missing leader.json to throw");
    }
    catch (const std::runtime_error& e)
    {
        CHECK(std::string(e.what()) == "Required faction config file missing: "
                                       + (factionDir / "leader.json").string());
    }

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("FactionConfigParser honors participates_in_council false", "[faction][parser]")
{
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "ac_faction_parser_council_test";
    std::filesystem::remove_all(tempDir);
    const std::filesystem::path factionDir = tempDir / "alien";
    std::filesystem::create_directories(factionDir);

    std::ofstream(factionDir / "identity.json") << R"({
        "name": "Alien",
        "participates_in_council": false
    })";
    std::ofstream(factionDir / "leader.json") << R"({"name": "X"})";
    std::ofstream(factionDir / "ai.json") << R"({})";
    std::ofstream(factionDir / "effects.json") << R"({"effects": []})";
    std::ofstream(factionDir / "base_names.json") << R"([])";
    std::ofstream(factionDir / "phrases.json") << R"({})";

    FactionConfigParser parser;
    const std::vector<FactionConfig_t> configs = parser.ParseConfig(tempDir.string());
    REQUIRE(configs.size() == 1);
    CHECK_FALSE(configs.front().identity.participatesInCouncil);

    std::filesystem::remove_all(tempDir);
}
