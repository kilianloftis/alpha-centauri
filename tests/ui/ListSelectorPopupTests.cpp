// The one list-selector, which replaced six forked copies. Every behaviour that had drifted
// between them lives here now, so it is tested once.

#include "RecordingGraphics.h"

#include "ui/ListSelectorPopup.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

// 100 units tall with 10-unit rows and a 2-row header: 8 lines below the header.
ListSelectorPopupStyle_t Style_()
{
    ListSelectorPopupStyle_t style{};
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
    const ListSelectorPopupStyle_t style = Style_();
    CHECK_THROWS_AS(
        ListSelectorPopup("Title", "Empty", Rows_(3), k_Layout, nullptr, style),
        std::invalid_argument);
}

TEST_CASE("Clicking a row reports its index", "[ui][selector]")
{
    const ListSelectorPopupStyle_t style = Style_();
    std::optional<size_t> selected;
    ListSelectorPopup popup("Title", "Empty", Rows_(3), k_Layout,
                            [&](size_t index) { selected = index; }, style);

    // Rows start at headerLineOffset * lineHeight = 20, each 10 tall.
    popup.HandleMouseClick(ClickAt_(5, 25));
    REQUIRE(selected.has_value());
    CHECK(*selected == 0);
    CHECK(popup.ShouldClose());
}

TEST_CASE("The popup is already closing when the handler runs", "[ui][selector]")
{
    // A handler may resume turn processing, and IGameView::HasModalElement / CanAdvanceTurn
    // count a modal that is not yet closing as a reason to refuse Advance. Selecting first and
    // closing after left such a handler's advance silently dropped.
    const ListSelectorPopupStyle_t style = Style_();
    bool bClosingDuringHandler = false;
    const ListSelectorPopup* pPopup = nullptr;
    ListSelectorPopup popup("Title", "Empty", Rows_(3), k_Layout,
                            [&](size_t) { bClosingDuringHandler = pPopup->ShouldClose(); }, style);
    pPopup = &popup;

    popup.HandleMouseClick(ClickAt_(5, 25));
    CHECK(bClosingDuringHandler);
}

TEST_CASE("A click outside the popup dismisses it without selecting", "[ui][selector]")
{
    // Modal routing delivers outside presses to the modal itself. Only one of the six copies
    // had this branch; the rest could only be closed with Escape.
    const ListSelectorPopupStyle_t style = Style_();
    bool bSelected = false;
    ListSelectorPopup popup("Title", "Empty", Rows_(3), k_Layout,
                            [&](size_t) { bSelected = true; }, style);

    popup.HandleMouseClick(ClickAt_(500, 500));
    CHECK(popup.ShouldClose());
    CHECK_FALSE(bSelected);
}

TEST_CASE("Escape closes the popup", "[ui][selector]")
{
    const ListSelectorPopupStyle_t style = Style_();
    ListSelectorPopup popup("Title", "Empty", Rows_(3), k_Layout, [](size_t) {}, style);

    CHECK(popup.HandleKey(KeyEvent_t{Key_t::Escape, {}}));
    CHECK(popup.ShouldClose());
}

TEST_CASE("A long list is bounded to the content area and scrolls", "[ui][selector]")
{
    // CacheEntryRects_ used to walk the whole vector against a fixed line height with no clamp,
    // so rows past the chrome were painted outside Contains() - neither visible nor clickable.
    const ListSelectorPopupStyle_t style = Style_();
    std::optional<size_t> selected;
    ListSelectorPopup popup("Title", "Empty", Rows_(20), k_Layout,
                            [&](size_t index) { selected = index; }, style);

    RecordingGraphics graphics;
    popup.Render(graphics);

    // Eight lines fit below the header, but the bottom one is reserved for the overflow
    // indicator, so seven rows are drawn and nothing beyond them.
    CHECK(graphics.TextYs("row6").size() == 1);
    CHECK(graphics.TextYs("row7").empty());
    // ...and the popup says there is more.
    CHECK(graphics.AnyTextContaining(" of 20"));

    // The indicator must not sit on top of a row: it used to be drawn at the last row's y,
    // where it overlapped that row's text and the row stayed clickable underneath it.
    const float indicatorY = graphics.FirstTextYContaining(" of 20");
    for (const std::string& rRow : {"row0", "row3", "row6"})
    {
        for (const float rowY : graphics.TextYs(rRow))
        {
            CHECK(rowY != indicatorY);
        }
    }

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
    const ListSelectorPopupStyle_t style = Style_();
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
    const ListSelectorPopupStyle_t style = Style_();
    ListSelectorPopup popup("Title", "Empty", Rows_(4), k_Layout, [](size_t) {}, style);

    CHECK_FALSE(popup.HandleKey(KeyEvent_t{Key_t::ArrowDown, {}}));

    RecordingGraphics graphics;
    popup.Render(graphics);
    CHECK_FALSE(graphics.AnyTextContaining(" of "));
}

TEST_CASE("An empty list shows its message and selects nothing", "[ui][selector]")
{
    const ListSelectorPopupStyle_t style = Style_();
    bool bSelected = false;
    ListSelectorPopup popup("Title", "Nothing here", {}, k_Layout,
                            [&](size_t) { bSelected = true; }, style);

    RecordingGraphics graphics;
    popup.Render(graphics);
    CHECK(graphics.TextYs("Nothing here").size() == 1);

    // A click inside the popup body must not invent a selection.
    popup.HandleMouseClick(ClickAt_(5, 25));
    CHECK_FALSE(bSelected);
}
