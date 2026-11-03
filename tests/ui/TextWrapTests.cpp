#include "RecordingGraphics.h"

#include "ui/TextWrap.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

// RecordingGraphics measures half the font size per character: 10 at size 20.
constexpr unsigned int k_FontSize = 20;

std::vector<std::string> Plain_(const std::vector<TextLine_t>& rLines)
{
    std::vector<std::string> plain;
    for (const TextLine_t& rLine : rLines)
    {
        std::string text;
        for (const TextRun_t& rRun : rLine)
        {
            text += rRun.text;
        }
        plain.push_back(text);
    }
    return plain;
}

} // namespace

TEST_CASE("Text wraps at spaces to fit the width", "[ui][text][wrap]")
{
    const RecordingGraphics graphics;
    const auto lines = WrapText(graphics, "one two three four", k_FontSize, 90.0f);
    CHECK(Plain_(lines) == std::vector<std::string>{"one two", "three", "four"});
}

TEST_CASE("SMAC's own line breaks stay", "[ui][text][wrap]")
{
    const RecordingGraphics graphics;
    const auto lines = WrapText(graphics, "a b\nc", k_FontSize, 1000.0f);
    CHECK(Plain_(lines) == std::vector<std::string>{"a b", "c"});
}

TEST_CASE("A word wider than the width gets a line to itself", "[ui][text][wrap]")
{
    const RecordingGraphics graphics;
    const auto lines = WrapText(graphics, "a unbreakable b", k_FontSize, 50.0f);
    CHECK(Plain_(lines) == std::vector<std::string>{"a", "unbreakable", "b"});
}

TEST_CASE("A highlight keeps its run across a line break", "[ui][text][wrap]")
{
    const RecordingGraphics graphics;
    const auto lines = WrapText(graphics, "pay {50 energy\ncredits} now", k_FontSize, 1000.0f);
    REQUIRE(lines.size() == 2);
    REQUIRE(lines[0].size() == 2);
    CHECK(lines[0][0].text == "pay");
    CHECK_FALSE(lines[0][0].bHighlight);
    CHECK(lines[0][1].text == " 50 energy");
    CHECK(lines[0][1].bHighlight);
    REQUIRE(lines[1].size() == 2);
    CHECK(lines[1][0].text == "credits");
    CHECK(lines[1][0].bHighlight);
    CHECK(lines[1][1].text == " now");
    CHECK_FALSE(lines[1][1].bHighlight);
}
