#include "ViewFixture.h"

#include "TestHelpers.h"
#include "game/Faction.h"
#include "game/buildings/BaseSpriteSizesConfig.h"
#include "game/buildings/BaseSpriteSizesConfigParser.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/MapOverlayChannelsConfig.h"
#include "game/buildings/MapOverlayChannelsConfigParser.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/map/Tile.h"
#include "ui/world/FactionBaseArt.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

using namespace ac;
using actest::ViewFixture;

namespace
{

BaseSpriteSizesConfig_t StockSizes_()
{
    return BaseSpriteSizesConfigParser{}.ParseConfig(actest::FixturePath("base_sprite_sizes.json"));
}

} // namespace

TEST_CASE("Faction sheet stems use faction.id when assets/factions/<id>/ exists",
          "[ui][faction-art]")
{
    ViewFixture withArt;
    withArt.factionDefinition.id = "gaian";
    std::filesystem::create_directories("assets/factions/gaian");
    CHECK(FactionSheetStem(*withArt.pPlayer) == "gaian");

    ViewFixture withoutArt;
    withoutArt.factionDefinition.id = "no_such_faction_art_dir";
    CHECK_FALSE(FactionSheetStem(*withoutArt.pPlayer).has_value());
}

TEST_CASE("Bare base sprite paths are land or water size stages only", "[ui][faction-art]")
{
    CHECK(BareBaseSpritePath("gaian", false, 1)
          == "assets/factions/gaian/bases/base_size1.png");
    CHECK(BareBaseSpritePath("hive", true, 3)
          == "assets/factions/hive/bases/water_base_size3.png");
    CHECK(BareBaseSpritePath("gaian", false, 5)
          == "assets/factions/gaian/bases/base_size5.png");
}

TEST_CASE("Map overlay path templates substitute faction and size", "[ui][faction-art]")
{
    CHECK(ResolveMapOverlayPathTemplate(
              "assets/factions/{faction}/bases/overlays/perimeter_size{size}.png", "gaian", 2)
          == "assets/factions/gaian/bases/overlays/perimeter_size2.png");
    CHECK(SubstituteMapOverlayFaction(
              "assets/factions/{faction}/bases/overlays/perimeter_size{size}.png", "gaian")
          == "assets/factions/gaian/bases/overlays/perimeter_size{size}.png");
}

TEST_CASE("Base sprite size stage follows base_sprite_sizes.json thresholds", "[ui][faction-art]")
{
    const BaseSpriteSizesConfig_t sizes = StockSizes_();
    CHECK(BaseSpriteSizeStage(1, 0, sizes) == 1);
    CHECK(BaseSpriteSizeStage(3, 0, sizes) == 1);
    CHECK(BaseSpriteSizeStage(4, 0, sizes) == 2);
    CHECK(BaseSpriteSizeStage(7, 0, sizes) == 2);
    CHECK(BaseSpriteSizeStage(8, 0, sizes) == 3);
    CHECK(BaseSpriteSizeStage(14, 0, sizes) == 3);
    CHECK(BaseSpriteSizeStage(15, 0, sizes) == 4);
    CHECK(BaseSpriteSizeStage(99, 0, sizes) == 4);
    CHECK(BaseSpriteSizeStage(3, 1, sizes) == 2);
    CHECK(BaseSpriteSizeStage(14, 1, sizes) == 4);
    CHECK(BaseSpriteSizeStage(15, 1, sizes) == 4);
}

TEST_CASE("A fifth size stage is reachable when configured", "[ui][faction-art]")
{
    BaseSpriteSizesConfig_t sizes = StockSizes_();
    sizes.sizeStages.push_back(BaseSpriteSizeStage_t{30});
    CHECK(BaseSpriteSizeStage(29, 0, sizes) == 4);
    CHECK(BaseSpriteSizeStage(30, 0, sizes) == 5);
    CHECK(BaseSpriteSizeStage(15, 1, sizes) == 5);
}

TEST_CASE("ResolveSizedSpritePath falls back to a lower stage when preferred is missing",
          "[ui][faction-art]")
{
    std::unordered_set<std::string> present = {"assets/x_size2.png"};
    const auto path = ResolveSizedSpritePath(
        4, [](int stage) { return "assets/x_size" + std::to_string(stage) + ".png"; },
        [&](const std::string& rPath) { return present.contains(rPath); }, "test art");
    REQUIRE(path);
    CHECK(*path == "assets/x_size2.png");
}

TEST_CASE("ResolveSizedSpritePath returns empty when no stage exists", "[ui][faction-art]")
{
    const auto path = ResolveSizedSpritePath(
        2, [](int stage) { return "assets/missing_size" + std::to_string(stage) + ".png"; },
        [](const std::string&) { return false; }, "missing art");
    CHECK_FALSE(path.has_value());
}

TEST_CASE("ResolveBaseMapOverlays keeps the higher-priority channel winner", "[ui][faction-art]")
{
    ViewFixture fixture;
    BuildingConfig_t high = fixture.dataContext.buildingRegistry->Get("Perimeter_Defense");
    high.id = "Tachyon_Field_Test";
    high.mapOverlayPriority = 2;
    high.mapOverlay->landPath =
        "assets/factions/{faction}/bases/overlays/tachyon_size{size}.png";

    // Assign before founding so BuildingManager pointers stay valid.
    std::vector<BuildingConfig_t> all = fixture.dataContext.buildingRegistry->GetAll();
    all.push_back(high);
    fixture.dataContext.buildingRegistry->Assign(std::move(all));

    BaseManager& rBase = fixture.MakeBase(8, 8);
    rBase.GetBuildingManager().AddBuilding("Perimeter_Defense");
    rBase.GetBuildingManager().AddBuilding("Tachyon_Field_Test");

    const auto overlays =
        ResolveBaseMapOverlays(rBase, "gaian", false, 1, *fixture.dataContext.mapOverlayChannels);
    REQUIRE(overlays.size() == 1);
    CHECK(overlays.front().buildingId == "Tachyon_Field_Test");
    CHECK(overlays.front().pathTemplate
          == "assets/factions/gaian/bases/overlays/tachyon_size{size}.png");
    CHECK(overlays.front().sizeStage == 1);
    CHECK(overlays.front().layer == 100);
}

TEST_CASE("Unchanneled overlays stack under a higher channel layer", "[ui][faction-art]")
{
    ViewFixture fixture;
    BuildingConfig_t deco;
    deco.id = "deco_overlay";
    deco.name = "Deco";
    deco.mapOverlay = BuildingMapOverlay_t{
        "assets/factions/{faction}/bases/overlays/deco_size{size}.png", ""};
    deco.mapOverlayLayer = 10;

    std::vector<BuildingConfig_t> all = fixture.dataContext.buildingRegistry->GetAll();
    all.push_back(deco);
    fixture.dataContext.buildingRegistry->Assign(std::move(all));

    BaseManager& rBase = fixture.MakeBase(8, 8);
    rBase.GetBuildingManager().AddBuilding("deco_overlay");
    rBase.GetBuildingManager().AddBuilding("Perimeter_Defense");

    const auto overlays =
        ResolveBaseMapOverlays(rBase, "gaian", false, 1, *fixture.dataContext.mapOverlayChannels);
    REQUIRE(overlays.size() == 2);
    CHECK(overlays[0].buildingId == "deco_overlay");
    CHECK(overlays[0].layer == 10);
    CHECK(overlays[1].buildingId == "Perimeter_Defense");
    CHECK(overlays[1].layer == 100);
}

TEST_CASE("BaseSpriteSizesConfigParser rejects non-increasing thresholds",
          "[ui][faction-art][parser]")
{
    const std::string path = "assets/factions/_test_bad_sizes/base_sprite_sizes.json";
    std::filesystem::create_directories("assets/factions/_test_bad_sizes");
    {
        std::ofstream out(path);
        out << R"({"size_stages":[{"min_population":1},{"min_population":1}]})";
    }
    CHECK_THROWS_WITH(BaseSpriteSizesConfigParser{}.ParseConfig(path),
                      Catch::Matchers::ContainsSubstring("strictly increasing"));
    std::filesystem::remove_all("assets/factions/_test_bad_sizes");
}

TEST_CASE("BaseSpriteSizesConfigParser reads optional origin_y_ratio per stage",
          "[ui][faction-art][parser]")
{
    const BaseSpriteSizesConfig_t sizes = StockSizes_();
    REQUIRE(sizes.sizeStages.size() >= 2);
    CHECK(sizes.sizeStages[0].originYRatio == Catch::Approx(-0.20f));
    CHECK(sizes.sizeStages[1].originYRatio == Catch::Approx(-0.12f));
}

TEST_CASE("LoadFactionColorsFile reads text and faction primary colours", "[ui][faction-art]")
{
    const std::string path = "assets/factions/_test_colors/colors.json";
    std::filesystem::create_directories("assets/factions/_test_colors");
    {
        std::ofstream out(path);
        out << R"({
  "faction_text_color_primary": {"palette_index": 1, "rgb": [10, 20, 30], "hex": "#0A141E"},
  "faction_color_primary": {"palette_index": 2, "rgb": [40, 50, 60], "hex": "#28323C"}
})";
    }

    const FactionColors_t colors = LoadFactionColorsFile(path);
    CHECK(colors.bHasTextPrimary);
    CHECK(colors.textPrimary.r == 10);
    CHECK(colors.textPrimary.g == 20);
    CHECK(colors.textPrimary.b == 30);
    CHECK(colors.LabelColor(Color_t::Yellow()).r == 10);

    std::filesystem::remove_all("assets/factions/_test_colors");
}

TEST_CASE("LoadFactionColorsFile rejects a broken colors.json", "[ui][faction-art]")
{
    const std::string path = "assets/factions/_test_bad_colors/colors.json";
    std::filesystem::create_directories("assets/factions/_test_bad_colors");
    {
        std::ofstream out(path);
        out << R"({"faction_color_primary": {"rgb": "not-an-array"}})";
    }

    CHECK_THROWS_WITH(LoadFactionColorsFile(path),
                      Catch::Matchers::ContainsSubstring("rgb"));
    std::filesystem::remove_all("assets/factions/_test_bad_colors");
}

TEST_CASE("Building parser requires map_overlay when a channel is named",
          "[ui][faction-art][parser]")
{
    BuildingConfigParser parser;
    const std::string path = "assets/factions/_test_overlay_parse/buildings.json";
    std::filesystem::create_directories("assets/factions/_test_overlay_parse");
    {
        std::ofstream out(path);
        out << R"([{
  "id": "channel_without_art",
  "name": "Bad",
  "map_overlay_channel": "base_defense"
}])";
    }
    CHECK_THROWS_WITH(parser.ParseConfig(path),
                      Catch::Matchers::ContainsSubstring("map_overlay"));
    std::filesystem::remove_all("assets/factions/_test_overlay_parse");
}

TEST_CASE("Unknown map_overlay_channel is rejected against the channel registry",
          "[ui][faction-art][parser]")
{
    BuildingConfigParser parser;
    const std::string path = "assets/factions/_test_bad_channel/buildings.json";
    std::filesystem::create_directories("assets/factions/_test_bad_channel");
    {
        std::ofstream out(path);
        out << R"([{
  "id": "bad_channel_bldg",
  "name": "Bad",
  "map_overlay": {"land": "x.png"},
  "map_overlay_channel": "no_such_channel"
}])";
    }
    const std::vector<BuildingConfig_t> buildings = parser.ParseConfig(path);
    const MapOverlayChannelsConfig_t channels =
        MapOverlayChannelsConfigParser{}.ParseConfig(
            actest::FixturePath("map_overlay_channels.json"));
    REQUIRE_FALSE(buildings.empty());
    CHECK_FALSE(channels.layersByChannel.contains(buildings.front().mapOverlayChannel));
    std::filesystem::remove_all("assets/factions/_test_bad_channel");
}
