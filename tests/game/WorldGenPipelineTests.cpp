#include "GameFixtures.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/LandmarkConfig.h"
#include "game/map/LandmarkConfigParser.h"
#include "game/map/LandmarkGeneration.h"
#include "game/map/RiverGeneration.h"
#include "game/map/Tile.h"
#include "game/map/WorldGenerator.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <vector>

using namespace ac;

namespace
{

std::filesystem::path TempPath_(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

MapGenerationConfig_t SmallMapConfig_()
{
    MapGenerationConfig_t config;
    config.width = 40;
    config.height = 30;
    config.oceanCoverage = 0.3f; // land-heavy, so landmarks and aquifers have room
    config.seed = 12345;         // deliberately non-zero: see the determinism case below
    return config;
}

WorldGenDecorationConfig_t DecorationWithRivers_()
{
    WorldGenDecorationConfig_t decoration;
    decoration.aquifers.landFraction = 0.05f;
    decoration.fungus.landFraction = 0.0f;
    decoration.tileBonuses.landFraction = 0.0f;
    return decoration;
}

// Landmarks that change the terrain rivers flow over: a sculpted peak (elevation) and a
// borehole cluster (terminates_river).
std::vector<LandmarkConfig_t> TerrainChangingLandmarks_()
{
    LandmarkConfig_t peak;
    peak.id = "TestPeak";
    peak.improvementId = "Monolith";
    peak.domain = LandmarkDomain_t::Land;
    peak.maxCount = 3;
    peak.minSpacing = 4;
    peak.shape.kind = LandmarkShapeKind_t::Sculptor;
    peak.shape.sculptorId = std::string(k_MountPlanetSculptor);
    peak.shape.radius = 3;

    LandmarkConfig_t boreholes;
    boreholes.id = "TestBoreholes";
    boreholes.improvementId = "ThermalBorehole";
    boreholes.domain = LandmarkDomain_t::Land;
    boreholes.maxCount = 6;
    boreholes.minSpacing = 3;
    boreholes.shape.kind = LandmarkShapeKind_t::Disk;
    boreholes.shape.radius = 1;

    return {peak, boreholes};
}

std::vector<bool> RiverFlags_(const WorldMap& rWorld)
{
    std::vector<bool> flags;
    for (const auto& pTile : rWorld.GetTiles())
    {
        flags.push_back(pTile->GetHasRiver());
    }
    return flags;
}

std::vector<int> Elevations_(const WorldMap& rWorld)
{
    std::vector<int> elevations;
    for (const auto& pTile : rWorld.GetTiles())
    {
        elevations.push_back(pTile->GetElevation());
    }
    return elevations;
}

} // namespace

TEST_CASE("Generated rivers are a fixed point of the terrain they were traced over",
          "[worldgen][rivers][pipeline]")
{
    // Rivers used to be traced before landmarks raised elevations and stamped
    // terminates_river features, so they flowed down pre-sculpt slopes and straight through
    // boreholes. Stated as an invariant of a correct pipeline: re-running the river pass on the
    // finished world must change nothing.
    actest::WorldFixture world;
    WorldGenerator generator;
    const std::unique_ptr<WorldMap> pWorld =
        generator.Generate(SmallMapConfig_(), WorldGenPresetConfig_t{}, DecorationWithRivers_(),
                           TerrainChangingLandmarks_(), world.improvements, 99u);

    // The landmarks have to have actually landed for this to mean anything.
    int boreholeTiles = 0;
    int riverTiles = 0;
    for (const auto& pTile : pWorld->GetTiles())
    {
        if (pTile->HasImprovement("ThermalBorehole"))
        {
            ++boreholeTiles;
        }
        if (pTile->GetHasRiver())
        {
            ++riverTiles;
        }
    }
    REQUIRE(boreholeTiles > 0);
    REQUIRE(riverTiles > 0);

    const std::vector<bool> asGenerated = RiverFlags_(*pWorld);
    RecomputeRivers(*pWorld);
    CHECK(RiverFlags_(*pWorld) == asGenerated);
}

TEST_CASE("The generated map is a function of the resolved seed, not the config seed",
          "[worldgen][seed]")
{
    // GenerateElevation_ re-read rConfig.seed to seed the noise field, so two sessions that
    // resolved different seeds from the same config produced the same terrain - the seed the
    // composition root reports could not reproduce the map it was reported for.
    actest::WorldFixture world;
    const MapGenerationConfig_t config = SmallMapConfig_();
    REQUIRE(config.seed != 0);

    WorldGenerator first;
    const std::unique_ptr<WorldMap> pFirst =
        first.Generate(config, WorldGenPresetConfig_t{}, DecorationWithRivers_(), {},
                       world.improvements, 1u);
    WorldGenerator second;
    const std::unique_ptr<WorldMap> pSecond =
        second.Generate(config, WorldGenPresetConfig_t{}, DecorationWithRivers_(), {},
                        world.improvements, 2u);
    WorldGenerator repeat;
    const std::unique_ptr<WorldMap> pRepeat =
        repeat.Generate(config, WorldGenPresetConfig_t{}, DecorationWithRivers_(), {},
                        world.improvements, 1u);

    CHECK(Elevations_(*pFirst) != Elevations_(*pSecond));
    CHECK(Elevations_(*pFirst) == Elevations_(*pRepeat));
    CHECK(RiverFlags_(*pFirst) == RiverFlags_(*pRepeat));
}

TEST_CASE("World generation places against bound tiles", "[worldgen][improvements]")
{
    // CanBuildImprovement reads a tile's terrain-feature configs to enforce the incumbent side
    // of `excludes`, and those are empty until BindImprovements. Unbound, world gen answered
    // coexistence questions differently from the same tile once it was in play - so a generated
    // map could contain a placement the rules forbid.
    actest::WorldFixture world;
    WorldGenerator generator;
    const std::unique_ptr<WorldMap> pWorld =
        generator.Generate(SmallMapConfig_(), WorldGenPresetConfig_t{}, DecorationWithRivers_(),
                           {}, world.improvements, 7u);

    for (const auto& pTile : pWorld->GetTiles())
    {
        REQUIRE_FALSE(pTile->GetTerrainFeatures().empty());
    }

    // The fixture's River excludes Mine, and rivers are generated. No generated tile may hold a
    // combination its own features reject.
    int riverTiles = 0;
    for (const auto& pTile : pWorld->GetTiles())
    {
        if (pTile->GetHasRiver())
        {
            ++riverTiles;
            CHECK_FALSE(CanBuildImprovement(*pTile, world.improvements.Get("Mine")));
        }
    }
    REQUIRE(riverTiles > 0);
}

TEST_CASE("Sculpt knobs come from landmark config", "[worldgen][landmarks][config]")
{
    const std::filesystem::path path = TempPath_("ac_landmarks_sculpt.json");
    {
        std::ofstream file(path);
        file << R"([{
            "id": "Volcano", "improvement_id": "Nutrients", "domain": "land",
            "max_count": 1, "min_spacing": 0,
            "shape": {
              "kind": "sculptor", "id": "mount_planet", "radius": 2,
              "sculpt": { "peak_elevation": 2000, "base_elevation": 500,
                          "rocky_core_radius": 0.0 }
            }
        }])" << '\n';
    }

    LandmarkConfigParser parser;
    const auto landmarks = parser.ParseConfig(path.string(), {"Nutrients"});
    std::filesystem::remove(path);

    REQUIRE(landmarks.size() == 1);
    const LandmarkSculpt_t& rSculpt = landmarks[0].shape.sculpt;
    CHECK(rSculpt.peakElevation == 2000);
    CHECK(rSculpt.baseElevation == 500);
    CHECK(rSculpt.rockyCoreRadius == 0.0f);

    actest::WorldFixture world(15, 15);
    for (const auto& pTile : world.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    std::mt19937 rng(7);
    REQUIRE(PlaceLandmarks(world.map, landmarks, world.improvements, rng) == 1);

    int peak = 0;
    bool bAnyRocky = false;
    for (const auto& pTile : world.map.GetTiles())
    {
        peak = std::max(peak, pTile->GetElevation());
        bAnyRocky = bAnyRocky || pTile->GetRockiness() == Rockiness_t::Rocky;
    }
    // Capped by the configured peak, not the former hardcoded 3500.
    CHECK(peak > 100);
    CHECK(peak <= 2000);
    // rocky_core_radius 0 means no tile is inside the core.
    CHECK_FALSE(bAnyRocky);
}

TEST_CASE("A landmark that can never be placed is rejected at load", "[worldgen][landmarks][config]")
{
    // An all-blank mask expands to no footprint; placement could only skip it in silence.
    const std::filesystem::path path = TempPath_("ac_landmarks_blank_mask.json");
    {
        std::ofstream file(path);
        file << R"([{
            "id": "Blank", "improvement_id": "Nutrients", "domain": "land",
            "shape": { "kind": "mask", "rows": ["   ", "   "] }
        }])" << '\n';
    }

    LandmarkConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string(), {"Nutrients"}),
                      Catch::Matchers::ContainsSubstring("Blank")
                          && Catch::Matchers::ContainsSubstring("footprint"));
    std::filesystem::remove(path);
}

TEST_CASE("An empty footprint reaching placement is an error, not a skip",
          "[worldgen][landmarks]")
{
    actest::WorldFixture world(11, 11);
    for (const auto& pTile : world.map.GetTiles())
    {
        pTile->SetElevation(100);
    }

    LandmarkConfig_t landmark;
    landmark.id = "Blank";
    landmark.improvementId = "Nutrients";
    landmark.domain = LandmarkDomain_t::Land;
    landmark.maxCount = 1;
    landmark.shape.kind = LandmarkShapeKind_t::Mask;
    landmark.shape.maskRows = {"   ", "   "};

    std::mt19937 rng(1);
    CHECK_THROWS_AS(PlaceLandmarks(world.map, {landmark}, world.improvements, rng),
                    std::runtime_error);
}
