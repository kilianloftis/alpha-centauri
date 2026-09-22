#include "game/buildings/BuildingConfigParser.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/research/TechConfigParser.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <variant>

using namespace ac;
using namespace actest;

TEST_CASE("TechConfigParser parses game categories", "[game][category][parser]")
{
    TechConfigParser parser;
    const std::vector<TechConfig_t> configs = parser.ParseConfig(FixturePath("techs.json"));

    REQUIRE(configs.size() == 19);
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
    CHECK(configs[8].category == GameCategory_t::Discover);
    CHECK(configs[8].id == "intellectual_integrity");
    CHECK(configs[9].id == "industrial_automation");
    REQUIRE(configs[9].effects.size() == 1);
    {
        const auto* pStat = std::get_if<StatModifierEffect_t>(&configs[9].effects[0].effect);
        REQUIRE(pStat != nullptr);
        CHECK(pStat->stat == StatId_t::CommerceRating);
        CHECK(pStat->amount == 1.0);
    }
    CHECK(configs[14].id == "sentient_econometrics");
    REQUIRE(configs[14].effects.size() == 1);
    {
        const auto* pStat = std::get_if<StatModifierEffect_t>(&configs[14].effects[0].effect);
        REQUIRE(pStat != nullptr);
        CHECK(pStat->stat == StatId_t::CommerceRating);
    }
    CHECK(configs[15].id == "graviton_theory");
    CHECK(configs[15].category == GameCategory_t::Discover);
    REQUIRE(configs[15].effects.size() == 1);
    {
        const auto* pFlag = std::get_if<RuleFlagEffect_t>(&configs[15].effects[0].effect);
        REQUIRE(pFlag != nullptr);
        CHECK(pFlag->flag == RuleFlagId_t::OrbitalInsertion);
    }
    CHECK(configs[16].id == "secrets_of_the_human_brain");
    CHECK(configs[16].category == GameCategory_t::Discover);
    REQUIRE(configs[16].onDiscoverEffects.size() == 1);
    CHECK(configs[17].id == "discover_chain_parent");
    CHECK(configs[18].id == "discover_chain_child");
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
