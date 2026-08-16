// Numeric spend dialog: finish-cost copy, a digit field pre-filled with that cost, OK / Cancel.

#include "RecordingGraphics.h"

#include "ui/base/HurryProductionPopup.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

HurryProductionPopupStyle_t Style_()
{
    HurryProductionPopupStyle_t style{};
    style.headerFontSizeRatio = 0.1f;
    style.entryFontSizeRatio = 0.1f;
    style.paddingRatio = 0.0f;
    style.buttonFontSizeRatio = 0.1f;
    style.fieldLayout = {0.1f, 0.4f, 0.8f, 0.2f};
    style.okButtonLayout = {0.0f, 0.8f, 0.4f, 0.2f};
    style.cancelButtonLayout = {0.6f, 0.8f, 0.4f, 0.2f};
    return style;
}

constexpr WindowLayout_t k_Layout{0.0f, 0.0f, 100.0f, 100.0f};

MouseEvent_t ClickAt_(int x, int y)
{
    return MouseEvent_t{MouseButton_t::Left, x, y, {}, true};
}

KeyEvent_t Key_(Key_t key)
{
    return KeyEvent_t{key, {}};
}

} // namespace

TEST_CASE("A hurry popup without a confirm handler is rejected at construction",
          "[ui][hurry]")
{
    const HurryProductionPopupStyle_t style = Style_();
    CHECK_THROWS_AS(HurryProductionPopup(k_Layout, 40, nullptr, style), std::invalid_argument);
}

TEST_CASE("A non-positive finish cost is rejected at construction", "[ui][hurry]")
{
    const HurryProductionPopupStyle_t style = Style_();
    CHECK_THROWS_AS(
        HurryProductionPopup(k_Layout, 0, [](int) {}, style), std::invalid_argument);
}

TEST_CASE("The field starts at the finish cost and OK reports it", "[ui][hurry]")
{
    const HurryProductionPopupStyle_t style = Style_();
    std::optional<int> spent;
    HurryProductionPopup popup(k_Layout, 40, [&](int credits) { spent = credits; }, style);

    RecordingGraphics graphics;
    popup.Render(graphics);
    CHECK(graphics.AnyTextContaining("Finishing construction costs 40 credits."));
    CHECK(graphics.AnyTextContaining("40|"));
    CHECK(graphics.AnyTextContaining("OK"));
    CHECK(graphics.AnyTextContaining("Cancel"));

    popup.HandleMouseClick(ClickAt_(20, 90));
    REQUIRE(spent.has_value());
    CHECK(*spent == 40);
    CHECK(popup.ShouldClose());
}

TEST_CASE("Cancel and Escape close without spending", "[ui][hurry]")
{
    const HurryProductionPopupStyle_t style = Style_();

    SECTION("Cancel")
    {
        std::optional<int> spent;
        HurryProductionPopup popup(k_Layout, 40, [&](int credits) { spent = credits; }, style);
        popup.HandleMouseClick(ClickAt_(80, 90));
        CHECK_FALSE(spent.has_value());
        CHECK(popup.ShouldClose());
    }

    SECTION("Escape")
    {
        std::optional<int> spent;
        HurryProductionPopup popup(k_Layout, 40, [&](int credits) { spent = credits; }, style);
        CHECK(popup.HandleKey(Key_(Key_t::Escape)));
        CHECK_FALSE(spent.has_value());
        CHECK(popup.ShouldClose());
    }
}

TEST_CASE("Digit keys and Backspace edit the amount OK will spend", "[ui][hurry]")
{
    const HurryProductionPopupStyle_t style = Style_();
    std::optional<int> spent;
    HurryProductionPopup popup(k_Layout, 40, [&](int credits) { spent = credits; }, style);

    CHECK(popup.HandleKey(Key_(Key_t::Backspace)));
    CHECK(popup.HandleKey(Key_(Key_t::Backspace)));
    CHECK(popup.HandleKey(Key_(Key_t::Num1)));
    CHECK(popup.HandleKey(Key_(Key_t::Num2)));
    CHECK(popup.HandleKey(Key_(Key_t::Enter)));

    REQUIRE(spent.has_value());
    CHECK(*spent == 12);
}

TEST_CASE("The caret moves with the arrows and digits insert at it", "[ui][hurry]")
{
    const HurryProductionPopupStyle_t style = Style_();
    std::optional<int> spent;
    HurryProductionPopup popup(k_Layout, 40, [&](int credits) { spent = credits; }, style);

    RecordingGraphics graphics;
    popup.HandleKey(Key_(Key_t::ArrowLeft));
    popup.Render(graphics);
    CHECK(graphics.AnyTextContaining("4|0"));

    graphics.texts.clear();
    popup.HandleKey(Key_(Key_t::Num9));
    popup.Render(graphics);
    CHECK(graphics.AnyTextContaining("49|0"));

    popup.HandleKey(Key_(Key_t::Enter));
    REQUIRE(spent.has_value());
    CHECK(*spent == 490);
}

TEST_CASE("Backspace deletes the digit before the caret", "[ui][hurry]")
{
    const HurryProductionPopupStyle_t style = Style_();
    std::optional<int> spent;
    HurryProductionPopup popup(k_Layout, 45, [&](int credits) { spent = credits; }, style);

    CHECK(popup.HandleKey(Key_(Key_t::ArrowLeft)));
    CHECK(popup.HandleKey(Key_(Key_t::Backspace)));
    popup.HandleKey(Key_(Key_t::Enter));
    REQUIRE(spent.has_value());
    CHECK(*spent == 5);
}

TEST_CASE("OK on an empty field closes without spending", "[ui][hurry]")
{
    const HurryProductionPopupStyle_t style = Style_();
    std::optional<int> spent;
    HurryProductionPopup popup(k_Layout, 9, [&](int credits) { spent = credits; }, style);
    CHECK(popup.HandleKey(Key_(Key_t::Backspace)));
    popup.HandleMouseClick(ClickAt_(20, 90));
    CHECK_FALSE(spent.has_value());
    CHECK(popup.ShouldClose());
}
