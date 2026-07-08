#include "game/buildings/BuildingConfigParser.h"
#include "game/research/TechConfigParser.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

TEST_CASE("TechConfigParser parses game categories", "[game][category][parser]")
{
    TechConfigParser parser;
    const std::vector<TechConfig_t> configs = parser.ParseConfig(FixturePath("techs.json"));

    REQUIRE(configs.size() == 5);
    CHECK(configs[0].category == GameCategory::Build);
    CHECK(configs[1].category == GameCategory::Grow);
    CHECK(configs[2].category == GameCategory::Discover);
    CHECK(configs[3].category == GameCategory::Conquer);
    CHECK(configs[4].category == GameCategory::Build);
}

TEST_CASE("BuildingConfigParser parses game categories", "[game][category][parser]")
{
    BuildingConfigParser parser;
    const std::vector<BuildingConfig_t> configs = parser.ParseConfig(FixturePath("buildings.json"));

    REQUIRE_FALSE(configs.empty());
    const BuildingConfig_t* pGrantedHall = nullptr;
    for (const BuildingConfig_t& rConfig : configs)
    {
        if (rConfig.id == "granted_hall")
        {
            pGrantedHall = &rConfig;
            break;
        }
    }

    REQUIRE(pGrantedHall != nullptr);
    CHECK(pGrantedHall->category == GameCategory::Discover);
}
