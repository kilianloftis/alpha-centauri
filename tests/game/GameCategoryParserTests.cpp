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

    REQUIRE(configs.size() == 8);
    CHECK(configs[0].category == GameCategory_t::Build);
    CHECK(configs[1].category == GameCategory_t::Grow);
    CHECK(configs[2].category == GameCategory_t::Discover);
    CHECK(configs[3].category == GameCategory_t::Conquer);
    CHECK(configs[4].category == GameCategory_t::Build);
    CHECK(configs[5].category == GameCategory_t::Build);
    CHECK(configs[5].effects.size() == 1);
    CHECK(configs[6].category == GameCategory_t::Discover);
    CHECK(configs[6].id == "fusion_power");
    CHECK(configs[6].effects.size() == 1);
    CHECK(configs[7].category == GameCategory_t::Discover);
    CHECK(configs[7].id == "quantum_power");
    CHECK(configs[7].effects.size() == 1);
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
    CHECK(pGrantedHall->category == GameCategory_t::Discover);
}
