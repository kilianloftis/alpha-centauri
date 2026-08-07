// The one list-selector, which replaced six forked copies. Every behaviour that had drifted
// between them lives here now, so it is tested once.

#include "graphics/Graphics.h"
#include "ui/ListSelectorPopup.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;

namespace
{

// Records nothing; the popup's geometry and routing are what these tests assert, not its pixels.
class StubGraphics_ : public Graphics
{
public:
    void PumpEvents() override {}
    void Clear() override {}
    void Display() override {}
    bool LoadTexture(const std::string&, const std::string&) override { return true; }
    bool DrawSprite(const std::string&, float, float) override { return true; }
    void DrawText(const std::string& rText, float, float, unsigned int, const Color_t&) override
    {
        drawnText.push_back(rText);
    }
    void DrawRect(float, float, float, float, const Color_t&, float) override {}
    void DrawFilledRect(float, float, float, float, const Color_t&) override {}
    void DrawLine(float, float, float, float, const Color_t&, float) override {}
    unsigned int GetWindowWidth() const override { return 1280; }
    unsigned int GetWindowHeight() const override { return 900; }

    std::vector<std::string> drawnText;
};

// 100 units tall with 10-unit rows and a 2-row header: 8 lines below the header.
ListSelectorPopupStyle Style_()
{
    ListSelectorPopupStyle style{};
    style.headerFontSizeRatio = 0.1f;
    style.entryFontSizeRatio = 0.1f;
    style.lineHeightRatio = 0.1f;
    style.paddingRatio = 0.0f;
    style.headerLineOffset = 2.0f;
    style.borderWidth = 1.0f;
    return style;
}

constexpr WindowLayout_t k_Layout{0.0f, 0.0f, 100.0f, 100.0f};

std::vector<std::string> Rows_(size_t count)
{
    std::vector<std::string> rows;
    for (size_t i = 0; i < count; ++i)
    {
        rows.push_back("row" + std::to_string(i));
    }
    return rows;
}

MouseEvent_t ClickAt_(int x, int y)
{
    return MouseEvent_t{MouseButton_t::Left, x, y, {}, true};
}

} // namespace

TEST_CASE("A selector without a handler is rejected at construction", "[ui][selector]")
{
    // Two of the six copies would silently do nothing on a click when the callback was empty,
    // leaving the popup open with no feedback.
    const ListSelectorPopupStyle style = Style_();
    CHECK_THROWS_AS(
        ListSelectorPopup("Title", "Empty", Rows_(3), k_Layout, nullptr, style),
        std::invalid_argument);
}

TEST_CASE("Clicking a row reports its index", "[ui][selector]")
{
    const ListSelectorPopupStyle style = Style_();
    std::optional<size_t> selected;
    ListSelectorPopup popup("Title", "Empty", Rows_(3), k_Layout,
                            [&](size_t index) { selected = index; }, style);

    // Rows start at headerLineOffset * lineHeight = 20, each 10 tall.
    popup.HandleMouseClick(ClickAt_(5, 25));
    REQUIRE(selected.has_value());
    CHECK(*selected == 0);
    CHECK(popup.ShouldClose());
}

TEST_CASE("A click outside the popup dismisses it without selecting", "[ui][selector]")
{
    // Modal routing delivers outside presses to the modal itself. Only one of the six copies
    // had this branch; the rest could only be closed with Escape.
    const ListSelectorPopupStyle style = Style_();
    bool bSelected = false;
    ListSelectorPopup popup("Title", "Empty", Rows_(3), k_Layout,
                            [&](size_t) { bSelected = true; }, style);

    popup.HandleMouseClick(ClickAt_(500, 500));
    CHECK(popup.ShouldClose());
    CHECK_FALSE(bSelected);
}

TEST_CASE("Escape closes the popup", "[ui][selector]")
{
    const ListSelectorPopupStyle style = Style_();
    ListSelectorPopup popup("Title", "Empty", Rows_(3), k_Layout, [](size_t) {}, style);

    CHECK(popup.HandleKey(KeyEvent_t{Key_t::Escape, {}}));
    CHECK(popup.ShouldClose());
}

TEST_CASE("A long list is bounded to the content area and scrolls", "[ui][selector]")
{
    // CacheEntryRects_ used to walk the whole vector against a fixed line height with no clamp,
    // so rows past the chrome were painted outside Contains() - neither visible nor clickable.
    const ListSelectorPopupStyle style = Style_();
    std::optional<size_t> selected;
    ListSelectorPopup popup("Title", "Empty", Rows_(20), k_Layout,
                            [&](size_t index) { selected = index; }, style);

    StubGraphics_ graphics;
    popup.Render(graphics);

    // Eight lines fit below the header, but the bottom one is reserved for the overflow
    // indicator, so seven rows are drawn and nothing beyond them.
    CHECK(std::count(graphics.drawnText.begin(), graphics.drawnText.end(), "row6") == 1);
    CHECK(std::count(graphics.drawnText.begin(), graphics.drawnText.end(), "row7") == 0);
    // ...and the popup says there is more.
    const bool bHasIndicator =
        std::any_of(graphics.drawnText.begin(), graphics.drawnText.end(),
                    [](const std::string& rText) { return rText.find(" of 20") != std::string::npos; });
    CHECK(bHasIndicator);

    // Scrolling brings later rows into reach, and a click reports the *absolute* index.
    for (int i = 0; i < 5; ++i)
    {
        CHECK(popup.HandleKey(KeyEvent_t{Key_t::ArrowDown, {}}));
    }
    popup.HandleMouseClick(ClickAt_(5, 25));
    REQUIRE(selected.has_value());
    CHECK(*selected == 5);
}

TEST_CASE("Scrolling stops at both ends of the list", "[ui][selector]")
{
    const ListSelectorPopupStyle style = Style_();
    ListSelectorPopup popup("Title", "Empty", Rows_(10), k_Layout, [](size_t) {}, style);

    // Already at the top.
    CHECK_FALSE(popup.HandleKey(KeyEvent_t{Key_t::ArrowUp, {}}));

    // Ten rows, seven visible once the indicator line is reserved: three steps, then no more.
    CHECK(popup.HandleKey(KeyEvent_t{Key_t::ArrowDown, {}}));
    CHECK(popup.HandleKey(KeyEvent_t{Key_t::ArrowDown, {}}));
    CHECK(popup.HandleKey(KeyEvent_t{Key_t::ArrowDown, {}}));
    CHECK_FALSE(popup.HandleKey(KeyEvent_t{Key_t::ArrowDown, {}}));
}

TEST_CASE("A list that fits does not scroll or advertise overflow", "[ui][selector]")
{
    const ListSelectorPopupStyle style = Style_();
    ListSelectorPopup popup("Title", "Empty", Rows_(4), k_Layout, [](size_t) {}, style);

    CHECK_FALSE(popup.HandleKey(KeyEvent_t{Key_t::ArrowDown, {}}));

    StubGraphics_ graphics;
    popup.Render(graphics);
    const bool bHasIndicator =
        std::any_of(graphics.drawnText.begin(), graphics.drawnText.end(),
                    [](const std::string& rText) { return rText.find(" of ") != std::string::npos; });
    CHECK_FALSE(bHasIndicator);
}

TEST_CASE("An empty list shows its message and selects nothing", "[ui][selector]")
{
    const ListSelectorPopupStyle style = Style_();
    bool bSelected = false;
    ListSelectorPopup popup("Title", "Nothing here", {}, k_Layout,
                            [&](size_t) { bSelected = true; }, style);

    StubGraphics_ graphics;
    popup.Render(graphics);
    CHECK(std::count(graphics.drawnText.begin(), graphics.drawnText.end(), "Nothing here") == 1);

    // A click inside the popup body must not invent a selection.
    popup.HandleMouseClick(ClickAt_(5, 25));
    CHECK_FALSE(bSelected);
}
