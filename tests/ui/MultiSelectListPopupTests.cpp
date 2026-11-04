#include "RecordingGraphics.h"

#include "ui/MultiSelectListPopup.h"

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

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

MouseEvent_t ClickAt_(int x, int y)
{
    return MouseEvent_t{MouseButton_t::Left, x, y, {}, true};
}

std::unique_ptr<MultiSelectListPopup> MakePopup_(size_t maxSelections,
                                                 std::function<void(std::vector<size_t>)> onConfirm)
{
    return std::make_unique<MultiSelectListPopup>(
        "Title", "Empty", std::vector<std::string>{"a", "b", "c"}, maxSelections, "Confirm",
        std::move(onConfirm), k_Layout, Style_(), nullptr);
}

} // namespace

TEST_CASE("MultiSelectListPopup rejects empty confirm or zero max", "[ui][multi_select]")
{
    CHECK_THROWS_AS(MakePopup_(0, [](std::vector<size_t>) {}), std::invalid_argument);
    CHECK_THROWS_AS(MakePopup_(2, {}), std::invalid_argument);
}

TEST_CASE("MultiSelectListPopup toggles then confirms selected indices", "[ui][multi_select]")
{
    std::optional<std::vector<size_t>> confirmed;
    auto pPopup = MakePopup_(2, [&](std::vector<size_t> selected) { confirmed = std::move(selected); });

    // Rows at y 20, 30, 40; Confirm at footer (height 100, two footer rows → y 80).
    pPopup->HandleMouseClick(ClickAt_(5, 25));
    pPopup->HandleMouseClick(ClickAt_(5, 35));
    pPopup->HandleMouseClick(ClickAt_(5, 45)); // over max; ignored
    CHECK_FALSE(confirmed.has_value());
    CHECK_FALSE(pPopup->ShouldClose());

    pPopup->HandleMouseClick(ClickAt_(5, 80));
    REQUIRE(confirmed.has_value());
    CHECK(*confirmed == std::vector<size_t>{0, 1});
    CHECK(pPopup->ShouldClose());
}

TEST_CASE("MultiSelectListPopup Cancel dismisses without confirming", "[ui][multi_select]")
{
    bool bConfirmed = false;
    auto pPopup = MakePopup_(2, [&](std::vector<size_t>) { bConfirmed = true; });
    pPopup->HandleMouseClick(ClickAt_(5, 25));
    pPopup->HandleMouseClick(ClickAt_(5, 90)); // Cancel footer row
    CHECK_FALSE(bConfirmed);
    CHECK(pPopup->ShouldClose());
}
