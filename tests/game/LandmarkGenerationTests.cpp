#include "GameFixtures.h"
#include "game/map/LandmarkConfigParser.h"
#include "game/map/LandmarkGeneration.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <random>

using namespace ac;

namespace
{

std::filesystem::path TempPath_(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

void FillLand_(WorldMap& rWorld, int elev = 1000)
{
    for (auto& pTile : rWorld.GetTiles())
    {
        pTile->SetElevation(elev);
    }
}

void FillWater_(WorldMap& rWorld, int elev = -1000)
{
    for (auto& pTile : rWorld.GetTiles())
    {
        pTile->SetElevation(elev);
    }
}

} // namespace

TEST_CASE("LandmarkConfigParser loads landmarks.json and validates improvement ids",
          "[worldgen][landmarks][parser]")
{
    actest::WorldFixture world;
    std::vector<std::string> ids;
    for (const ImprovementConfig_t& rConfig : world.improvements.GetAll())
    {
        ids.push_back(rConfig.id);
    }

    // Production landmarks.json references production improvements; use a tiny fixture file.
    const std::filesystem::path path = TempPath_("ac_landmarks_ok.json");
    {
        std::ofstream file(path);
        file << R"([
  {
    "id": "TestJungle",
    "improvement_id": "Nutrients",
    "domain": "land",
    "max_count": 1,
    "min_spacing": 4,
    "shape": { "kind": "disk", "radius": 1 }
  }
])" << '\n';
    }

    LandmarkConfigParser parser;
    const auto landmarks = parser.ParseConfig(path.string(), ids);
    REQUIRE(landmarks.size() == 1);
    CHECK(landmarks[0].improvementId == "Nutrients");
    CHECK(landmarks[0].domain == LandmarkDomain_t::Land);
    CHECK(landmarks[0].shape.kind == LandmarkShapeKind_t::Disk);
    CHECK(landmarks[0].shape.radius == 1);

    std::filesystem::remove(path);
}

TEST_CASE("LandmarkConfigParser loads production landmarks.json against production improvements",
          "[worldgen][landmarks][parser]")
{
    const std::string configRoot =
        std::string(AC_TEST_FIXTURES_DIR) + "/../../config";
    ImprovementRegistry improvements;
    improvements.Load(configRoot + "/improvements.json");
    std::vector<std::string> ids;
    for (const ImprovementConfig_t& rConfig : improvements.GetAll())
    {
        ids.push_back(rConfig.id);
    }

    LandmarkConfigParser parser;
    const auto landmarks = parser.ParseConfig(configRoot + "/worldGen/landmarks.json", ids);
    CHECK_FALSE(landmarks.empty());
    bool foundFossil = false;
    for (const LandmarkConfig_t& rLandmark : landmarks)
    {
        if (rLandmark.id == "FossilFieldRidge")
        {
            foundFossil = true;
            CHECK(rLandmark.domain == LandmarkDomain_t::Water);
            CHECK(rLandmark.shape.kind == LandmarkShapeKind_t::Mask);
        }
    }
    CHECK(foundFossil);
}

TEST_CASE("LandmarkConfigParser throws on unknown improvement_id", "[worldgen][landmarks][parser]")
{
    const std::filesystem::path path = TempPath_("ac_landmarks_bad_id.json");
    {
        std::ofstream file(path);
        file << R"([{"id":"X","improvement_id":"NoSuchThing","domain":"land",
            "shape":{"kind":"disk","radius":0}}])" << '\n';
    }

    LandmarkConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string(), {"Nutrients"}),
                      Catch::Matchers::ContainsSubstring("NoSuchThing"));
    std::filesystem::remove(path);
}

TEST_CASE("ExpandLandmarkShape disk and mask", "[worldgen][landmarks]")
{
    LandmarkShape_t disk;
    disk.kind = LandmarkShapeKind_t::Disk;
    disk.radius = 0;
    CHECK(ExpandLandmarkShape(disk).size() == 1);

    disk.radius = 1;
    // Euclidean radius 1: center + 4 orthogonals (+ diagonals? 1+1=2 > 1+1=2 → dx^2+dy^2 <= 2)
    // (1,1): 2 <= 2, so diagonals included → 9 cells
    CHECK(ExpandLandmarkShape(disk).size() == 9);

    LandmarkShape_t mask;
    mask.kind = LandmarkShapeKind_t::Mask;
    mask.maskRows = {" X ", "XXX", " X "};
    const auto cells = ExpandLandmarkShape(mask);
    CHECK(cells.size() == 5);
}

TEST_CASE("PlaceLandmarks respects water domain for FossilFieldRidge-style stamp",
          "[worldgen][landmarks]")
{
    actest::WorldFixture world(21, 21);
    FillWater_(world.map);

    LandmarkConfig_t landmark;
    landmark.id = "Fossil";
    landmark.improvementId = "Nutrients";
    landmark.domain = LandmarkDomain_t::Water;
    landmark.maxCount = 1;
    landmark.minSpacing = 0;
    landmark.shape.kind = LandmarkShapeKind_t::Disk;
    landmark.shape.radius = 1;

    std::mt19937 rng(1);
    const int placed = PlaceLandmarks(world.map, {landmark}, world.improvements, rng);
    CHECK(placed == 1);

    int stamped = 0;
    for (const auto& pTile : world.map.GetTiles())
    {
        if (pTile->HasImprovement("Nutrients"))
        {
            ++stamped;
            CHECK(pTile->IsWater());
        }
    }
    CHECK(stamped == 9);
}

TEST_CASE("PlaceLandmarks fails water landmark on all-land map", "[worldgen][landmarks]")
{
    actest::WorldFixture world(11, 11);
    FillLand_(world.map);

    LandmarkConfig_t landmark;
    landmark.id = "Fossil";
    landmark.improvementId = "Nutrients";
    landmark.domain = LandmarkDomain_t::Water;
    landmark.maxCount = 1;
    landmark.minSpacing = 0;
    landmark.shape.kind = LandmarkShapeKind_t::Disk;
    landmark.shape.radius = 1;

    std::mt19937 rng(1);
    CHECK(PlaceLandmarks(world.map, {landmark}, world.improvements, rng) == 0);
}

TEST_CASE("PlaceLandmarks stamps land landmark yield improvement", "[worldgen][landmarks]")
{
    actest::WorldFixture world(15, 15);
    FillLand_(world.map);

    LandmarkConfig_t landmark;
    landmark.id = "Jungle";
    landmark.improvementId = "Nutrients";
    landmark.domain = LandmarkDomain_t::Land;
    landmark.maxCount = 1;
    landmark.minSpacing = 0;
    landmark.shape.kind = LandmarkShapeKind_t::Disk;
    landmark.shape.radius = 0;

    std::mt19937 rng(42);
    REQUIRE(PlaceLandmarks(world.map, {landmark}, world.improvements, rng) == 1);

    int stamped = 0;
    for (const auto& pTile : world.map.GetTiles())
    {
        if (pTile->HasImprovement("Nutrients"))
        {
            ++stamped;
        }
    }
    CHECK(stamped == 1);
}
