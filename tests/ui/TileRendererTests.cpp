#include "RecordingGraphics.h"

#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

void EnsureStyleLoaded_()
{
    static const bool bLoaded = []
    {
        UiStyle::Load(std::string(AC_CONFIG_DIR) + "/ui/style.json");
        return true;
    }();
    (void)bLoaded;
}

bool ColorEq_(const Color_t& a, const Color_t& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool HasFilledColor_(const RecordingGraphics& rGraphics, const Color_t& color)
{
    for (const RecordingGraphics::RectDraw_t& rRect : rGraphics.rects)
    {
        if (rRect.bFilled && ColorEq_(rRect.color, color))
        {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("TileRenderer paints moisture/rockiness instead of numeric placeholders",
          "[ui][tile]")
{
    EnsureStyleLoaded_();
    const auto& s = Style().tileRenderer;

    SECTION("moist rocky land gets a grey ring and green center, no text")
    {
        Tile tile(0, 0);
        tile.SetElevation(500);
        tile.SetMoisture(Moisture_t::Moist);
        tile.SetRockiness(Rockiness_t::Rocky);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, 10.0f, 20.0f, 100.0f, /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK(HasFilledColor_(graphics, s.rockyRingColor));
        CHECK(HasFilledColor_(graphics, s.moistCenterColor));
    }

    SECTION("wet rolling land uses the lighter grey and darker green")
    {
        Tile tile(1, 1);
        tile.SetElevation(200);
        tile.SetMoisture(Moisture_t::Wet);
        tile.SetRockiness(Rockiness_t::Rolling);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, 0.0f, 0.0f, 80.0f, /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK(HasFilledColor_(graphics, s.rollingRingColor));
        CHECK(HasFilledColor_(graphics, s.wetCenterColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.rockyRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.moistCenterColor));
    }

    SECTION("water keeps the elevation fill and skips landform overlays")
    {
        Tile tile(2, 2);
        tile.SetElevation(-1500);
        tile.SetMoisture(Moisture_t::Wet);
        tile.SetRockiness(Rockiness_t::Rocky);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, 0.0f, 0.0f, 64.0f, /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK_FALSE(HasFilledColor_(graphics, s.rockyRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.rollingRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.moistCenterColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.wetCenterColor));
        REQUIRE_FALSE(graphics.rects.empty());
        CHECK(graphics.rects.front().bFilled);
        CHECK(ColorEq_(graphics.rects.front().color,
                       TileRenderer::FillColor(tile, /*bFogged*/ false)));
    }

    SECTION("deeper water is darker than shallower water on the elevation gradient")
    {
        Tile deep(4, 4);
        deep.SetElevation(k_MinElevation);
        Tile shallow(5, 5);
        shallow.SetElevation(-1);

        const Color_t deepFill = TileRenderer::FillColor(deep, /*bFogged*/ false);
        const Color_t shallowFill = TileRenderer::FillColor(shallow, /*bFogged*/ false);
        const int deepLuma = deepFill.r + deepFill.g + deepFill.b;
        const int shallowLuma = shallowFill.r + shallowFill.g + shallowFill.b;
        CHECK(deepLuma < shallowLuma);
        CHECK(ColorEq_(deepFill, s.waterLowColor));
        CHECK(ColorEq_(shallowFill, s.waterHighColor));
    }

    SECTION("arid flat land is brown fill only")
    {
        Tile tile(3, 3);
        tile.SetElevation(100);
        tile.SetMoisture(Moisture_t::Arid);
        tile.SetRockiness(Rockiness_t::Flat);

        RecordingGraphics graphics;
        TileRenderer::Render(graphics, tile, 0.0f, 0.0f, 50.0f, /*bFogged*/ false);

        CHECK(graphics.texts.empty());
        CHECK_FALSE(HasFilledColor_(graphics, s.rockyRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.rollingRingColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.moistCenterColor));
        CHECK_FALSE(HasFilledColor_(graphics, s.wetCenterColor));
    }
}
