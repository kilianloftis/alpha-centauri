#include "lib/LuaRuntime.h"
#include "lib/Rational.h"
#include "lib/Registry.h"
#include "lib/config/ConfigFields.h"

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

    std::vector<ThingConfig_t> ParseConfig(const std::string&) { return staged; }
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

TEST_CASE("ParseStringArray rejects a value of the wrong shape", "[lib][config]")
{
    // A present-but-wrong-typed value read identically to an absent key, so
    // "prerequisites": "tech_x" silently meant no prerequisites at all.
    SECTION("absent is an empty list")
    {
        const nlohmann::json j = nlohmann::json::object();
        CHECK(ConfigFields::ParseStringArray(j, "tags").empty());
    }

    SECTION("a bare string is an error, not an empty list")
    {
        const nlohmann::json j = {{"tags", "not_an_array"}};
        CHECK_THROWS_WITH(ConfigFields::ParseStringArray(j, "tags"),
                          Catch::Matchers::ContainsSubstring("tags"));
    }

    SECTION("an array with a non-string entry is an error")
    {
        const nlohmann::json j = {{"tags", {"ok", 7}}};
        CHECK_THROWS_WITH(ConfigFields::ParseStringArray(j, "tags"),
                          Catch::Matchers::ContainsSubstring("tags"));
    }

    SECTION("an array of strings parses")
    {
        const nlohmann::json j = {{"tags", {"a", "b"}}};
        const std::vector<std::string> tags = ConfigFields::ParseStringArray(j, "tags");
        REQUIRE(tags.size() == 2);
        CHECK(tags[0] == "a");
        CHECK(tags[1] == "b");
    }
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
