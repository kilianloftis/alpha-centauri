#include "RecordingGraphics.h"
#include "ViewFixture.h"

#include "ui/SpriteLibrary.h"
#include "ui/style/DrawPanelChrome.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;

namespace
{

// Minimal valid 1×1 PNG (RGBA) for Ensure to load.
void WriteTinyPng_(const std::filesystem::path& path)
{
    static constexpr unsigned char k_Png[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
        0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f,
        0x15, 0xc4, 0x89, 0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x00,
        0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x49,
        0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(k_Png), sizeof(k_Png));
}

} // namespace

TEST_CASE("DrawPanelChrome fills when the sprite path is empty", "[ui][chrome]")
{
    RecordingGraphics graphics;
    SpriteLibrary sprites(graphics, [](const std::string&) { return false; });
    const WindowLayout_t layout{10.0f, 20.0f, 100.0f, 50.0f};
    const Color_t fill{1, 2, 3, 255};
    const Color_t border{4, 5, 6, 255};

    DrawPanelChrome(graphics, &sprites, layout, fill, "", border, 2.0f);

    REQUIRE(graphics.rects.size() == 2);
    CHECK(graphics.rects[0].bFilled);
    CHECK(graphics.rects[0].x == 10.0f);
    CHECK(graphics.rects[0].color.r == 1);
    CHECK_FALSE(graphics.rects[1].bFilled);
    CHECK(graphics.sprites.empty());
}

TEST_CASE("DrawPanelChrome draws a native-size sprite when Ensure succeeds", "[ui][chrome]")
{
    RecordingGraphics graphics;
    const std::filesystem::path png =
        std::filesystem::temp_directory_path() / "ac_chrome_test" / "panel.png";
    WriteTinyPng_(png);
    SpriteLibrary sprites(graphics, [png](const std::string& path) { return path == png.string(); });
    const WindowLayout_t layout{10.0f, 20.0f, 100.0f, 50.0f};

    DrawPanelChrome(graphics, &sprites, layout, Color_t::Black(), png.string(), Color_t::White(),
                    1.0f);

    REQUIRE(graphics.sprites.size() == 1);
    CHECK(graphics.sprites.front().textureId == png.string());
    CHECK(graphics.sprites.front().x == 10.0f);
    CHECK(graphics.sprites.front().y == 20.0f);
    CHECK_FALSE(graphics.sprites.front().bScaled);
    REQUIRE_FALSE(graphics.rects.empty());
    CHECK_FALSE(graphics.rects.back().bFilled);
}

TEST_CASE("Map base name uses separate X/Y offset ratios under the footprint", "[ui][map][chrome]")
{
    ViewFixture fixture;
    CHECK(Style().mapRenderer.baseNameOffsetYRatio
          > Style().mapRenderer.baseNameOffsetXRatio);
    CHECK(Style().mapRenderer.baseNameOffsetYRatio > 0.5f);
}
