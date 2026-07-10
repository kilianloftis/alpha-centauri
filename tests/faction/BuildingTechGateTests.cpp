#include "game/buildings/BuildingConfigParser.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;

namespace
{

std::filesystem::path WriteTempBuildingJson(const std::string& body)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_building_tech_gate.json";
    std::ofstream out(path);
    out << "[\n" << body << "\n]\n";
    return path;
}

} // namespace

TEST_CASE("Building with no required_tech is always available", "[buildings][tech-gate]")
{
    BuildingConfig_t config;
    config.id = "recycling_tanks";
    config.requiredTech = "";

    CHECK(config.IsAvailable({}));
    CHECK(config.IsAvailable({"unrelated_tech"}));
}

TEST_CASE("Building with required_tech needs that tech discovered", "[buildings][tech-gate]")
{
    BuildingConfig_t config;
    config.id = "nutrient_bank";
    config.requiredTech = "ecology";

    CHECK_FALSE(config.IsAvailable({}));
    CHECK_FALSE(config.IsAvailable({"biogenetics"}));
    CHECK(config.IsAvailable({"ecology"}));
    CHECK(config.IsAvailable({"biogenetics", "ecology"}));
}

TEST_CASE("BuildingConfigParser reads singular required_tech", "[buildings][tech-gate][parser]")
{
    const std::filesystem::path path = WriteTempBuildingJson(R"(
  {
    "id": "gated_hall",
    "name": "Gated Hall",
    "mineral_cost": 10,
    "required_tech": "ecology",
    "category": "grow"
  }
)");

    BuildingConfigParser parser;
    const std::vector<BuildingConfig_t> configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 1);
    CHECK(configs[0].requiredTech == "ecology");
    CHECK_FALSE(configs[0].IsAvailable({}));
    CHECK(configs[0].IsAvailable({"ecology"}));

    std::filesystem::remove(path);
}

TEST_CASE("BuildingConfigParser treats omitted required_tech as always available",
          "[buildings][tech-gate][parser]")
{
    const std::filesystem::path path = WriteTempBuildingJson(R"(
  {
    "id": "starter_hall",
    "name": "Starter Hall",
    "mineral_cost": 5,
    "category": "grow"
  }
)");

    BuildingConfigParser parser;
    const std::vector<BuildingConfig_t> configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 1);
    CHECK(configs[0].requiredTech.empty());
    CHECK(configs[0].IsAvailable({}));

    std::filesystem::remove(path);
}

TEST_CASE("BuildingConfigParser rejects legacy required_techs array", "[buildings][tech-gate][parser]")
{
    const std::filesystem::path path = WriteTempBuildingJson(R"(
  {
    "id": "legacy_hall",
    "name": "Legacy Hall",
    "mineral_cost": 5,
    "required_techs": ["ecology"],
    "category": "grow"
  }
)");

    BuildingConfigParser parser;
    CHECK_THROWS_AS(parser.ParseConfig(path.string()), std::runtime_error);

    std::filesystem::remove(path);
}
