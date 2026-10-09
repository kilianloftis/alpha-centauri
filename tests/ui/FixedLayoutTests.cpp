#include "TempConfigFile.h"
#include "TestHelpers.h"
#include "ViewFixture.h"

#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <iterator>
#include <string>

using namespace ac;
using actest::TempConfigFile;
using actest::ViewFixture;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace
{

FixedLayout_t MakeConsoleSpec_()
{
    FixedLayout_t spec{};
    spec.alignX = 0.5f;
    spec.alignY = 1.0f;
    spec.variants = {
        FixedLayoutVariant_t{1024.0f, 257.0f, 45.0f, -1408.0f,
                             "assets/ui/chrome/console2.png"},
        FixedLayoutVariant_t{800.0f, 257.0f, 45.0f, -1520.0f,
                             "assets/ui/chrome/console.png"},
    };
    return spec;
}

std::string ReplaceBracketArray_(std::string text, const std::string& keyPrefix,
                                 const std::string& replacement)
{
    const size_t start = text.find(keyPrefix);
    REQUIRE(start != std::string::npos);
    const size_t open = text.find('[', start);
    REQUIRE(open != std::string::npos);
    int depth = 0;
    size_t end = open;
    for (; end < text.size(); ++end)
    {
        if (text[end] == '[')
        {
            ++depth;
        }
        else if (text[end] == ']')
        {
            --depth;
            if (depth == 0)
            {
                break;
            }
        }
    }
    REQUIRE(depth == 0);
    text.replace(open, end - open + 1, replacement);
    return text;
}

} // namespace

TEST_CASE("PlaceFixedLayout picks the widest variant that fits", "[ui][layout]")
{
    const FixedLayout_t spec = MakeConsoleSpec_();

    SECTION("1280-wide window uses console2, centered on the bottom")
    {
        const WindowLayout_t parent{0.0f, 0.0f, 1280.0f, 900.0f};
        const FixedPlacement_t placed = PlaceFixedLayout(parent, spec);
        CHECK(placed.layout.width == 1024.0f);
        CHECK(placed.layout.height == 257.0f);
        CHECK_THAT(placed.layout.x, WithinAbs(128.0f, 1e-4f));
        CHECK_THAT(placed.layout.y, WithinAbs(643.0f, 1e-4f));
        CHECK(placed.sprite == "assets/ui/chrome/console2.png");
        CHECK(placed.mapOverlap == 45.0f);
        CHECK(placed.spriteOffsetX == -1408.0f);

        const WindowLayout_t map = MapBandAbove(parent, spec);
        CHECK(map.x == 0.0f);
        CHECK(map.y == 0.0f);
        CHECK(map.width == 1280.0f);
        CHECK_THAT(map.height, WithinAbs(643.0f + 45.0f, 1e-3f));
    }

    SECTION("900-wide window uses console.png, centered")
    {
        const WindowLayout_t parent{0.0f, 0.0f, 900.0f, 900.0f};
        const FixedPlacement_t placed = PlaceFixedLayout(parent, spec);
        CHECK(placed.layout.width == 800.0f);
        CHECK_THAT(placed.layout.x, WithinAbs(50.0f, 1e-4f));
        CHECK(placed.sprite == "assets/ui/chrome/console.png");
        CHECK(placed.spriteOffsetX == -1520.0f);
    }

    SECTION("640-wide window picks the narrowest and clips")
    {
        const WindowLayout_t parent{0.0f, 0.0f, 640.0f, 900.0f};
        const FixedPlacement_t placed = PlaceFixedLayout(parent, spec);
        CHECK(placed.layout.width == 800.0f);
        CHECK_THAT(placed.layout.x, WithinAbs(-80.0f, 1e-4f));
        CHECK(placed.sprite == "assets/ui/chrome/console.png");
    }
}

TEST_CASE("console_layouts ratios stay inside the placed console", "[ui][layout]")
{
    ViewFixture fixture;
    const WindowLayout_t parent = ViewFixture::FullScreen();
    const FixedPlacement_t placed = PlaceFixedLayout(parent, Style().layouts.console);
    const WindowLayout_t unit =
        ResolveLayout(placed.layout, Style().worldView.consoleUnit);

    CHECK(unit.x >= placed.layout.x);
    CHECK(unit.y >= placed.layout.y);
    CHECK(unit.x + unit.width <= placed.layout.x + placed.layout.width + 1e-3f);
    CHECK(unit.y + unit.height <= placed.layout.y + placed.layout.height + 1e-3f);
}

TEST_CASE("Fixed console layout rejects empty or invalid variants", "[config][ui]")
{
    std::ifstream in(actest::FixturePath("ui/style.json"));
    REQUIRE(in.good());
    const std::string style((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

    SECTION("empty variants")
    {
        const std::string mutated = ReplaceBracketArray_(style, "\"variants\":", "[]");
        TempConfigFile config("ac_style_empty_console_variants.json", mutated);
        CHECK_THROWS_WITH(UiStyle::Load(config.Path()), ContainsSubstring("variants"));
    }

    SECTION("non-positive variant size")
    {
        std::string mutated = style;
        const size_t at = mutated.find("\"width\": 1024");
        REQUIRE(at != std::string::npos);
        mutated.replace(at, std::string("\"width\": 1024").size(), "\"width\": 0");
        TempConfigFile config("ac_style_zero_console_width.json", mutated);
        CHECK_THROWS_WITH(UiStyle::Load(config.Path()), ContainsSubstring("variant size"));
    }

    SECTION("align outside [0, 1]")
    {
        std::string mutated = style;
        const std::string from = "\"align\": [\n        0.5, 1.0\n      ]";
        const size_t at = mutated.find(from);
        REQUIRE(at != std::string::npos);
        mutated.replace(at, from.size(), "\"align\": [\n        0.5, 1.5\n      ]");
        TempConfigFile config("ac_style_bad_console_align.json", mutated);
        CHECK_THROWS_WITH(UiStyle::Load(config.Path()), ContainsSubstring("align"));
    }

    SECTION("map_overlap above height")
    {
        std::string mutated = style;
        const size_t at = mutated.find("\"map_overlap\": 45");
        REQUIRE(at != std::string::npos);
        mutated.replace(at, std::string("\"map_overlap\": 45").size(),
                        "\"map_overlap\": 300");
        TempConfigFile config("ac_style_bad_map_overlap.json", mutated);
        CHECK_THROWS_WITH(UiStyle::Load(config.Path()), ContainsSubstring("map_overlap"));
    }
}
