#include "game/map/ImprovementConfigParser.h"
#include "game/map/TerrainConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace ac;

namespace
{

std::filesystem::path WriteTempJson(const std::string& name, const std::string& body)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << body;
    return path;
}

} // namespace

TEST_CASE("ImprovementConfigParser rejects a legacy terraform block", "[improvements][parser]")
{
    // Construction now lives in turns_required / energy_cost on the improvement itself.
    const auto path = WriteTempJson("ac_improvement_parser.json", R"([
        {
            "id": "Road",
            "name": "Road",
            "terraform": { "improvement": "Road" },
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("turns_required"));
    std::filesystem::remove(path);
}

TEST_CASE("ImprovementConfigParser: rejects mineral_cost", "[improvements][parser]")
{
    const auto path = WriteTempJson("ac_improvement_mineral.json", R"([
        { "id": "Road", "name": "Road", "mineral_cost": 5, "effects": [] }
    ])");

    ImprovementConfigParser parser;
    CHECK_THROWS(parser.ParseConfig(path.string()));
    std::filesystem::remove(path);
}

TEST_CASE("An improvement is buildable exactly when it declares turns_required",
          "[improvements][parser][orders]")
{
    const auto path = WriteTempJson("ac_buildable.json", R"([
        { "id": "Farm", "name": "Farm", "turns_required": 4, "energy_cost": 0 },
        { "id": "Base", "name": "Base" }
    ])");
    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 2);
    CHECK(configs[0].placement == OccupantPlacement_t::Improvement);
    REQUIRE(configs[0].project);
    CHECK(configs[0].project->turnsRequired == 4);
    CHECK(IsBuildable(configs[0]));
    CHECK_FALSE(IsBuildable(configs[1]));
    std::filesystem::remove(path);
}

TEST_CASE("An energy cost with no build time is rejected", "[improvements][parser][orders]")
{
    // Nothing can ever pay it, so it is a config mistake rather than an unbuildable entry.
    const auto path = WriteTempJson("ac_cost_no_time.json", R"([
        { "id": "Farm", "name": "Farm", "energy_cost": 5 }
    ])");
    ImprovementConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("turns_required"));
    std::filesystem::remove(path);
}

TEST_CASE("A terrain operation must declare what it does", "[improvements][parser][terrain]")
{
    // The operation set is open, so nothing checks an id against a list in code. What keeps a
    // typo from becoming a project that silently does nothing is the effects list itself.
    const auto path = WriteTempJson("ac_terrain_ops_empty.json", R"({
        "operations": [
            { "id": "Dance", "name": "Dance", "turns_required": 1, "energy_cost": 0 }
        ]
    })");
    TerrainOperationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("Dance")
                          && Catch::Matchers::ContainsSubstring("on_complete_effects"));
    std::filesystem::remove(path);
}

TEST_CASE("A terrain operation composed of existing effects parses",
          "[improvements][parser][terrain]")
{
    // An id no C++ enum names: the point of the open set is that this is ordinary config.
    const auto path = WriteTempJson("ac_terrain_ops_custom.json", R"({
        "operations": [
            {
                "id": "Rockify",
                "name": "Rockify",
                "turns_required": 4,
                "energy_cost": 0,
                "on_complete_effects": [
                    { "type": "StepRockiness", "parameters": { "steps": 1 } }
                ]
            }
        ]
    })");
    TerrainOperationConfigParser parser;
    const std::vector<TerrainOperationConfig_t> ops = parser.ParseConfig(path.string());
    REQUIRE(ops.size() == 1);
    CHECK(ops[0].id == "Rockify");
    REQUIRE(ops[0].onCompleteEffects.size() == 1);
    const auto* pStep = std::get_if<StepRockinessEffect_t>(&ops[0].onCompleteEffects[0].effect);
    REQUIRE(pStep);
    CHECK(pStep->steps == 1);
    std::filesystem::remove(path);
}

TEST_CASE("@buildable stands for every former-built improvement", "[improvements][parser]")
{
    // One entry states "clear me before building anything here", instead of every buildable
    // improvement restating the relationship and a new one silently forgetting to.
    std::vector<ImprovementConfig_t> occupants;
    ImprovementConfig_t farm;
    farm.id = "Farm";
    farm.project = FormerProject_t{4, 0, ""};
    ImprovementConfig_t base;
    base.id = "Base";
    ImprovementConfig_t fungus;
    fungus.id = "Fungus";
    fungus.placement = OccupantPlacement_t::Terrain;
    fungus.excludes = {"@buildable"};
    occupants.push_back(farm);
    occupants.push_back(base);
    occupants.push_back(fungus);

    ExpandFeatureTagReferences(occupants);

    // Farm is buildable; Base declares no project, so founding a base on fungus is untouched.
    CHECK(occupants[2].excludes == std::vector<std::string>{"Farm"});
}

TEST_CASE("@buildable cannot be authored by hand", "[improvements][parser]")
{
    std::vector<ImprovementConfig_t> occupants;
    ImprovementConfig_t odd;
    odd.id = "Odd";
    odd.tags = {"buildable"};
    occupants.push_back(odd);

    CHECK_THROWS_WITH(ExpandFeatureTagReferences(occupants),
                      Catch::Matchers::ContainsSubstring("reserved")
                          && Catch::Matchers::ContainsSubstring("turns_required"));
}

TEST_CASE("suppress_terrain expands tags and rejects ids no occupant has",
          "[improvements][parser]")
{
    std::vector<ImprovementConfig_t> occupants;
    ImprovementConfig_t fungus;
    fungus.id = "Fungus";
    fungus.tags = {"bloom"};
    ImprovementConfig_t ocean;
    ocean.id = "Ocean";
    ocean.suppressTerrain = {"@bloom"};
    occupants.push_back(fungus);
    occupants.push_back(ocean);

    ExpandFeatureTagReferences(occupants);
    CHECK(occupants[1].suppressTerrain == std::vector<std::string>{"Fungus"});

    occupants[1].suppressTerrain = {"Kraken"};
    CHECK_THROWS_WITH(ExpandFeatureTagReferences(occupants),
                      Catch::Matchers::ContainsSubstring("suppress_terrain")
                          && Catch::Matchers::ContainsSubstring("Kraken"));
}

TEST_CASE("ImprovementConfigParser: suppress_yield_sources", "[improvements][parser]")
{
    const auto path = WriteTempJson("ac_improvement_suppress.json", R"([
        {
            "id": "Forest",
            "name": "Forest",
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
    const auto path = WriteTempJson("ac_improvement_tags.json", R"([
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
    const auto path = WriteTempJson("ac_improvement_tag_expand.json", R"([
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
    const auto path = WriteTempJson("ac_improvement_tag_self.json", R"([
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
    const auto path = WriteTempJson("ac_improvement_tag_dedupe.json", R"([
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
    const auto path = WriteTempJson("ac_improvement_bad_tag.json", R"([
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
    // Unexpanded: the fixture's improvements name tags that only terrain entries carry, so
    // expanding against improvements.json alone would not resolve them.
    const auto configs =
        ParseImprovementsUnexpanded(std::string(AC_TEST_FIXTURES_DIR) + "/improvements.json");
    REQUIRE(configs.size() > 10);

    const ImprovementConfig_t* pFarm = nullptr;
    for (const auto& c : configs)
    {
        if (c.id == "Farm")
        {
            pFarm = &c;
        }
    }
    REQUIRE(pFarm);
    CHECK(pFarm->excludes == std::vector<std::string>({"Rocky"}));

    REQUIRE(pFarm->project);
    CHECK(pFarm->project->turnsRequired == 4);
    CHECK(IsBuildable(*pFarm));
}

TEST_CASE("sprite_paths lists art per tile surface", "[improvements][parser]")
{
    const auto path = WriteTempJson("ac_improvement_sprites.json", R"([
        {
            "id": "Kelp",
            "name": "Kelp",
            "sprite_paths": { "land": ["a.png"], "sea": ["b.png", "c.png"] },
            "effects": []
        },
        {
            "id": "Grove",
            "name": "Grove",
            "sprite_paths": { "land": ["d.png"] },
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 2);
    CHECK(configs[0].spritePaths.land == std::vector<std::string>{"a.png"});
    CHECK(configs[0].spritePaths.sea == std::vector<std::string>{"b.png", "c.png"});
    CHECK(configs[1].spritePaths.land == std::vector<std::string>{"d.png"});
    CHECK(configs[1].spritePaths.sea.empty());
    std::filesystem::remove(path);
}

TEST_CASE("sprite_paths rejects a surface that is not land or sea", "[improvements][parser]")
{
    const auto path = WriteTempJson("ac_improvement_sprite_surface.json", R"([
        {
            "id": "Grove",
            "name": "Grove",
            "sprite_paths": { "water": ["d.png"] },
            "effects": []
        }
    ])");

    ImprovementConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("Grove")
                          && Catch::Matchers::ContainsSubstring("water"));
    std::filesystem::remove(path);
}

TEST_CASE("sprite_overhang_ratio defaults to zero and must not be negative",
          "[improvements][parser]")
{
    const auto path = WriteTempJson("ac_improvement_overhang.json", R"([
        { "id": "Tall", "name": "Tall", "sprite_overhang_ratio": 0.24, "effects": [] },
        { "id": "Flat", "name": "Flat", "effects": [] }
    ])");
    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 2);
    CHECK(configs[0].spriteOverhangRatio == 0.24f);
    CHECK(configs[1].spriteOverhangRatio == 0.0f);
    std::filesystem::remove(path);

    const auto negative = WriteTempJson("ac_improvement_overhang_negative.json", R"([
        { "id": "Sunken", "name": "Sunken", "sprite_overhang_ratio": -0.1, "effects": [] }
    ])");
    CHECK_THROWS_WITH(parser.ParseConfig(negative.string()),
                      Catch::Matchers::ContainsSubstring("Sunken")
                          && Catch::Matchers::ContainsSubstring("sprite_overhang_ratio"));
    std::filesystem::remove(negative);
}

TEST_CASE("sprite_tiles names a tile set per surface", "[improvements][parser]")
{
    const auto path = WriteTempJson("ac_improvement_tiles.json", R"([
        {
            "id": "Grove",
            "name": "Grove",
            "sprite_tiles": { "layout": "edges", "land": "grove/{mask}.png" },
            "effects": []
        },
        {
            "id": "Bloom",
            "name": "Bloom",
            "sprite_tiles": { "layout": "blob", "land": "a/{mask}.png", "sea": "b/{mask}.png" },
            "effects": []
        },
        { "id": "Plain", "name": "Plain", "effects": [] }
    ])");

    ImprovementConfigParser parser;
    const auto configs = parser.ParseConfig(path.string());
    REQUIRE(configs.size() == 3);
    REQUIRE(configs[0].spriteTiles.has_value());
    CHECK(configs[0].spriteTiles->layout == SpriteTileLayout_t::Edges);
    CHECK(configs[0].spriteTiles->land == "grove/{mask}.png");
    CHECK(configs[0].spriteTiles->sea.empty());
    REQUIRE(configs[1].spriteTiles.has_value());
    CHECK(configs[1].spriteTiles->layout == SpriteTileLayout_t::Blob);
    CHECK(configs[1].spriteTiles->sea == "b/{mask}.png");
    CHECK_FALSE(configs[2].spriteTiles.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("sprite_tiles rejects a pattern without {mask}, an unknown layout, or both art kinds",
          "[improvements][parser]")
{
    ImprovementConfigParser parser;
    const struct
    {
        const char* name;
        const char* body;
        const char* message;
    } k_Cases[] = {
        {"ac_tiles_no_mask.json",
         R"([{ "id": "Grove", "name": "Grove",
               "sprite_tiles": { "layout": "edges", "land": "grove.png" }, "effects": [] }])",
         "{mask}"},
        {"ac_tiles_layout.json",
         R"([{ "id": "Grove", "name": "Grove",
               "sprite_tiles": { "layout": "corners", "land": "g/{mask}.png" }, "effects": [] }])",
         "layout"},
        {"ac_tiles_both.json",
         R"([{ "id": "Grove", "name": "Grove", "sprite_paths": { "land": ["g.png"] },
               "sprite_tiles": { "layout": "edges", "land": "g/{mask}.png" }, "effects": [] }])",
         "sprite_tiles"},
    };
    for (const auto& rCase : k_Cases)
    {
        CAPTURE(rCase.name);
        const auto path = WriteTempJson(rCase.name, rCase.body);
        CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                          Catch::Matchers::ContainsSubstring("Grove")
                              && Catch::Matchers::ContainsSubstring(rCase.message));
        std::filesystem::remove(path);
    }
}
