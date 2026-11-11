#include "lib/LuaRuntime.h"
#include "lib/Rational.h"
#include "lib/Registry.h"
#include "lib/config/ConfigReader.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;

namespace
{

struct ThingConfig_t
{
    std::string id;
    bool bRejected = false;
};

// Parses whatever the test staged, so Load can be driven without touching the filesystem.
struct StagedParser
{
    static inline std::vector<ThingConfig_t> staged;

    static std::vector<ThingConfig_t> ParseConfig(const std::string&) { return staged; }
};

class ThingRegistry : public Registry<ThingConfig_t, StagedParser>
{
protected:
    void Validate_() override
    {
        Registry::Validate_();
        for (const ThingConfig_t& rConfig : m_configs)
        {
            if (rConfig.bRejected)
            {
                throw std::runtime_error("rejected '" + rConfig.id + "'");
            }
        }
    }
};

} // namespace

TEST_CASE("A rejected config file leaves the previous registry contents intact", "[lib][registry]")
{
    // Load replaced m_configs before validating, so a bad file destroyed a working registry:
    // the object was left holding exactly the payload that had just been refused.
    ThingRegistry registry;
    StagedParser::staged = {{"good_a"}, {"good_b"}};
    REQUIRE_NOTHROW(registry.Load("ignored"));
    REQUIRE(registry.GetAll().size() == 2);

    StagedParser::staged = {{"replacement"}, {"bad", /*bRejected=*/true}};
    CHECK_THROWS_WITH(registry.Load("ignored"), Catch::Matchers::ContainsSubstring("bad"));

    CHECK(registry.GetAll().size() == 2);
    CHECK(registry.Find("good_a") != nullptr);
    CHECK(registry.Find("good_b") != nullptr);
    CHECK(registry.Find("replacement") == nullptr);
    CHECK(registry.Find("bad") == nullptr);
}

TEST_CASE("ConfigReader::StringArray rejects a value of the wrong shape", "[lib][config]")
{
    // A present-but-wrong-typed value read identically to an absent key, so
    // "prerequisites": "tech_x" silently meant no prerequisites at all.
    SECTION("absent is an empty list")
    {
        const nlohmann::json j = nlohmann::json::object();
        CHECK(ConfigReader(j, "test").StringArray("tags").empty());
    }

    SECTION("a bare string is an error, not an empty list")
    {
        const nlohmann::json j = {{"tags", "not_an_array"}};
        CHECK_THROWS_WITH(ConfigReader(j, "test").StringArray("tags"),
                          Catch::Matchers::ContainsSubstring("tags"));
    }

    SECTION("an array with a non-string entry is an error")
    {
        const nlohmann::json j = {{"tags", {"ok", 7}}};
        CHECK_THROWS_WITH(ConfigReader(j, "test").StringArray("tags"),
                          Catch::Matchers::ContainsSubstring("tags"));
    }

    SECTION("an array of strings parses")
    {
        const nlohmann::json j = {{"tags", {"a", "b"}}};
        const std::vector<std::string> tags = ConfigReader(j, "test").StringArray("tags");
        REQUIRE(tags.size() == 2);
        CHECK(tags[0] == "a");
        CHECK(tags[1] == "b");
    }
}

TEST_CASE("ConfigReader number fields accept numbers, numeric strings and fractions",
          "[lib][config]")
{
    const nlohmann::json j = {
        {"as_number", 2.5},
        {"as_int", 3},
        {"as_string", "4.5"},
        {"as_fraction", "1/3"},
        {"bad", true},
        {"junk", "2abc"},
        {"bad_fraction", "1/0"},
        {"trailing_fraction", "1/3x"},
    };
    const ConfigReader reader(j, "test");

    CHECK(reader.Number("as_number") == Catch::Approx(2.5));
    CHECK(reader.Number("as_int") == Catch::Approx(3.0));
    CHECK(reader.Number("as_string") == Catch::Approx(4.5));
    CHECK(reader.Number("as_fraction") == Catch::Approx(1.0 / 3.0));
    CHECK(reader.Float("as_fraction") == Catch::Approx(1.0f / 3.0f));
    CHECK(reader.NumberOr("missing", 7.0) == Catch::Approx(7.0));
    CHECK(reader.FloatOr("as_fraction", 0.0f) == Catch::Approx(1.0f / 3.0f));
    CHECK_THROWS_WITH(reader.Number("bad"), Catch::Matchers::ContainsSubstring("'bad'"));
    CHECK_THROWS_WITH(reader.Number("junk"), Catch::Matchers::ContainsSubstring("2abc"));
    CHECK_THROWS(reader.Number("bad_fraction"));
    CHECK_THROWS(reader.Number("trailing_fraction"));
    CHECK_THROWS_WITH(reader.Number("missing"), Catch::Matchers::ContainsSubstring("required"));
}

TEST_CASE("ConfigReader errors name the context and the key's path", "[lib][config]")
{
    const nlohmann::json j = {{"outer", {{"inner", -1}, {"list", {{{"x", "y"}}}}}}};
    const ConfigReader reader(j, "thing config 'thing.json'");
    const ConfigReader outer = reader.Object("outer");

    CHECK_THROWS_WITH(outer.IntAtLeast("inner", 0),
                      Catch::Matchers::ContainsSubstring("thing config 'thing.json'")
                          && Catch::Matchers::ContainsSubstring("'outer.inner' must be >= 0"));
    const std::vector<ConfigReader> entries = outer.Objects("list");
    REQUIRE(entries.size() == 1);
    CHECK_THROWS_WITH(entries[0].Int("x"),
                      Catch::Matchers::ContainsSubstring("'outer.list[0].x' must be an integer"));
    CHECK_THROWS_WITH(outer.RejectUnknownKeys({"inner"}),
                      Catch::Matchers::ContainsSubstring("unknown key 'outer.list'"));
}

TEST_CASE("Rational_t::Parse rejects text that is not a whole fraction", "[lib][rational]")
{
    // stoi stopped at the first non-digit, so "0.5" read as 0 and "1/3x" as 1/3.
    CHECK(Rational_t::Parse("2").numerator == 2);
    CHECK(Rational_t::Parse("-1/3").numerator == -1);
    CHECK(Rational_t::Parse("-1/3").denominator == 3);
    CHECK_THROWS(Rational_t::Parse("0.5"));
    CHECK_THROWS(Rational_t::Parse("1/3x"));
    CHECK_THROWS(Rational_t::Parse("/3"));
    CHECK_THROWS(Rational_t::Parse("1/0"));
}

TEST_CASE("Rational_t::ScaledInt reports overflow instead of wrapping", "[lib][rational]")
{
    // num * (scale / den) was computed in int, so the product overflowed - UB, and a wrong
    // answer rather than the "must scale exactly" error the API otherwise gives.
    const Rational_t huge{std::numeric_limits<int>::max() / 2, 1};
    CHECK_THROWS_WITH(huge.ScaledInt(4), Catch::Matchers::ContainsSubstring("does not fit"));

    CHECK(Rational_t{1, 3}.ScaledInt(9) == 3);
    CHECK(Rational_t{2, 1}.ScaledInt(3) == 6);
    CHECK_THROWS(Rational_t{1, 3}.ScaledInt(4));
}

TEST_CASE("Rational_t scales with truncation or rounding up", "[lib][rational]")
{
    CHECK(Rational_t{1, 3}.ScaledTruncated(301) == 100);
    CHECK(Rational_t{3, 2}.ScaledTruncated(101) == 151);
    CHECK(Rational_t{3, 2}.ScaledTruncated(-101) == -151);
    CHECK(Rational_t{0, 1}.ScaledTruncated(500) == 0);

    CHECK(Rational_t{1, 2}.ScaledRoundedUp(7) == 4);
    CHECK(Rational_t{1, 2}.ScaledRoundedUp(8) == 4);
    CHECK(Rational_t{1, 2}.ScaledRoundedUp(-7) == -3);

    const Rational_t huge{std::numeric_limits<int>::max() / 2, 1};
    CHECK_THROWS_WITH(huge.ScaledTruncated(4), Catch::Matchers::ContainsSubstring("does not fit"));
}

TEST_CASE("LuaRuntime::EvalInt fails loudly rather than returning zero", "[lib][lua]")
{
    LuaRuntime lua;

    SECTION("an empty formula is a config error")
    {
        // Returned 0, which TechCostCalculator's floor then turned into research cost 1.
        CHECK_THROWS_WITH(lua.EvalInt("", {}), Catch::Matchers::ContainsSubstring("empty"));
    }

    SECTION("a broken formula carries the Lua diagnostic")
    {
        CHECK_THROWS_WITH(lua.EvalInt("this is not lua", {}),
                          Catch::Matchers::ContainsSubstring("this is not lua"));
    }

    SECTION("a fractional result is rejected")
    {
        CHECK_THROWS_WITH(lua.EvalInt("7 / 2", {}),
                          Catch::Matchers::ContainsSubstring("whole number"));
        CHECK(lua.EvalInt("floor(7 / 2)", {}) == 3);
    }

    SECTION("fractional inputs are accepted when the result is whole")
    {
        CHECK(lua.EvalInt("floor(factor * 8 + 0.5)", {{"factor", 0.5}}) == 4);
        CHECK(lua.EvalInt("floor(bureaucracy * 0.5 + 0.5)", {{"bureaucracy", 12.0}}) == 6);
    }

    SECTION("a non-numeric result is rejected")
    {
        CHECK_THROWS_WITH(lua.EvalInt("'text'", {}),
                          Catch::Matchers::ContainsSubstring("did not return a number"));
    }
}

TEST_CASE("LuaRuntime::EvalInt does not leak variables between calls", "[lib][lua]")
{
    // Variables were written as globals and never cleared, so a formula that omitted an input
    // silently read the previous formula's value for it.
    LuaRuntime lua;

    CHECK(lua.EvalInt("size * 2", {{"size", 5}}) == 10);
    CHECK_THROWS(lua.EvalInt("size * 2", {}));

    // The failing call must not leave its own inputs behind either.
    CHECK_THROWS(lua.EvalInt("nope +", {{"leftover", 3}}));
    CHECK_THROWS(lua.EvalInt("leftover", {}));
}

TEST_CASE("LuaRuntime::EvalInt reuses a compiled formula without reusing its inputs",
          "[lib][lua]")
{
    // The chunk for a formula is compiled once and kept; it must still read whatever globals
    // the current call set, not the ones it was first compiled against.
    LuaRuntime lua;

    CHECK(lua.EvalInt("size * 2", {{"size", 5}}) == 10);
    CHECK(lua.EvalInt("size * 2", {{"size", 21}}) == 42);
    CHECK(lua.EvalInt("size * 2", {{"size", 0}}) == 0);

    // A formula that does not compile is not kept, so it fails the same way every time.
    CHECK_THROWS_WITH(lua.EvalInt("still not lua", {}),
                      Catch::Matchers::ContainsSubstring("still not lua"));
    CHECK_THROWS_WITH(lua.EvalInt("still not lua", {}),
                      Catch::Matchers::ContainsSubstring("still not lua"));
}
