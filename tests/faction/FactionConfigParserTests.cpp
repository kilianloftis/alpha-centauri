#include "game/faction/FactionConfigParser.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace ac;
using Catch::Approx;

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
    CHECK(pComplete->ai.wealth == Approx(1.0f));
    CHECK(pComplete->ai.power == Approx(0.0f));
    CHECK(pComplete->ai.growth == Approx(0.3f));
    CHECK(pComplete->ai.tech == Approx(0.9f));
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
    CHECK(pMinimal->leader.name == "Leader");
    CHECK(pMinimal->leader.title.empty());
    CHECK(pMinimal->ai.wealth == Approx(0.5f));
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
