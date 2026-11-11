#include "TestHelpers.h"
#include "TestTempDir.h"

#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyConfigParser.h"
#include "ui/StandingCaptions.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace ac;
using namespace actest;

namespace
{

DiplomacyConfig_t FixtureDiplomacy_()
{
    return DiplomacyConfigParser::ParseConfig(FixturePath("diplomacy.json"));
}

StandingCaptions LoadFixture_()
{
    return StandingCaptions::Load(FixturePath("standing_captions.json"), FixtureDiplomacy_());
}

StandingCaptions LoadEdited_(const std::string& rName,
                             const std::function<void(nlohmann::json&)>& rEdit)
{
    std::ifstream in(FixturePath("standing_captions.json"));
    nlohmann::json json = nlohmann::json::parse(in);
    rEdit(json);
    const std::filesystem::path path = TestTempDir::Path() / rName;
    std::ofstream(path) << json.dump();
    return StandingCaptions::Load(path.string(), FixtureDiplomacy_());
}

} // namespace

TEST_CASE("Mood names each friction band of the caption table", "[ui][standing]")
{
    const StandingCaptions captions = LoadFixture_();

    CHECK(captions.Mood(0) == "Magnanimous");
    CHECK(captions.Mood(1) == "Solicitous");
    CHECK(captions.Mood(2) == "Solicitous");
    CHECK(captions.Mood(3) == "Cooperative");
    CHECK(captions.Mood(4) == "Cooperative");
    CHECK(captions.Mood(5) == "Noncommittal");
    CHECK(captions.Mood(8) == "Noncommittal");
    CHECK(captions.Mood(9) == "Ambivalent");
    CHECK(captions.Mood(12) == "Ambivalent");
    CHECK(captions.Mood(13) == "Obstinate");
    CHECK(captions.Mood(15) == "Obstinate");
    CHECK(captions.Mood(16) == "Quarrelsome");
    CHECK(captions.Mood(17) == "Quarrelsome");
    CHECK(captions.Mood(18) == "Belligerent");
    CHECK(captions.Mood(19) == "Belligerent");
    CHECK(captions.Mood(20) == "Seething");
}

TEST_CASE("Repute names each blemish count, and counts past the last band read the last band",
          "[ui][standing]")
{
    const StandingCaptions captions = LoadFixture_();

    CHECK(captions.Repute(0) == "Noble");
    CHECK(captions.Repute(1) == "Faithful");
    CHECK(captions.Repute(2) == "Scrupulous");
    CHECK(captions.Repute(3) == "Dependable");
    CHECK(captions.Repute(4) == "Ruthless");
    CHECK(captions.Repute(5) == "Treacherous");
    CHECK(captions.Repute(6) == "Wicked");
    CHECK(captions.Repute(7) == "Infamous");
    CHECK(captions.Repute(8) == "Infamous");
    CHECK(captions.Repute(99) == "Infamous");
}

TEST_CASE("The caption table requires both band lists, each a non-empty array",
          "[ui][standing]")
{
    const std::string key = GENERATE(std::string("moods"), std::string("repute"));

    CHECK_THROWS_AS(LoadEdited_("ac_captions_missing_" + key + ".json",
                                [&key](nlohmann::json& j) { j.erase(key); }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_empty_" + key + ".json",
                                [&key](nlohmann::json& j) { j[key] = nlohmann::json::array(); }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_object_" + key + ".json",
                                [&key](nlohmann::json& j) { j[key] = nlohmann::json::object(); }),
                    std::runtime_error);
}

TEST_CASE("A band list starts at 0 and rises strictly", "[ui][standing]")
{
    const auto [key, minKey] = GENERATE(table<std::string, std::string>(
        {{"moods", "min_friction"}, {"repute", "min_blemishes"}}));

    CHECK_THROWS_AS(LoadEdited_("ac_captions_start_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][0][minKey] = 1; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_flat_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][1][minKey] = 0; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_falling_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][2][minKey] = 0; }),
                    std::runtime_error);
}

TEST_CASE("The last band starts at or below the stored maximum", "[ui][standing]")
{
    const DiplomacyConfig_t diplomacy = FixtureDiplomacy_();

    CHECK_THROWS_AS(LoadEdited_("ac_captions_mood_above_max.json",
                                [&diplomacy](nlohmann::json& j) {
                                    j["moods"].back()["min_friction"] = diplomacy.frictionMax + 1;
                                }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_repute_above_max.json",
                                [&diplomacy](nlohmann::json& j) {
                                    j["repute"].back()["min_blemishes"] =
                                        diplomacy.blemishesMax + 1;
                                }),
                    std::runtime_error);
    CHECK_NOTHROW(LoadEdited_("ac_captions_mood_at_max.json", [&diplomacy](nlohmann::json& j) {
        j["moods"].back()["min_friction"] = diplomacy.frictionMax;
    }));
}

TEST_CASE("Each band has a non-empty name, an integer min and no other keys", "[ui][standing]")
{
    const auto [key, minKey] = GENERATE(table<std::string, std::string>(
        {{"moods", "min_friction"}, {"repute", "min_blemishes"}}));

    CHECK_THROWS_AS(LoadEdited_("ac_captions_no_name_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][1].erase("name"); }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_empty_name_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][1]["name"] = ""; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_number_name_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][1]["name"] = 5; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_no_min_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][1].erase(minKey); }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_string_min_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][1][minKey] = "1"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_extra_key_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][1]["label"] = "x"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("ac_captions_not_object_" + key + ".json",
                                [&](nlohmann::json& j) { j[key][1] = "Solicitous"; }),
                    std::runtime_error);
}

TEST_CASE("The caption table rejects an unknown top-level key", "[ui][standing]")
{
    CHECK_THROWS_AS(LoadEdited_("ac_captions_unknown_key.json",
                                [](nlohmann::json& j) { j["vendetta_floor"] = 15; }),
                    std::runtime_error);
}

TEST_CASE("The talk caption floors friction at Vendetta only", "[ui][standing]")
{
    const StandingCaptions captions = LoadFixture_();

    // The fixture floor is 15, inside Obstinate.
    CHECK(captions.CaptionMood(0, DiplomaticStatus_t::Vendetta) == "Obstinate");
    CHECK(captions.CaptionMood(14, DiplomaticStatus_t::Vendetta) == "Obstinate");
    CHECK(captions.CaptionMood(18, DiplomaticStatus_t::Vendetta) == "Belligerent");
    CHECK(captions.CaptionMood(0, DiplomaticStatus_t::Neutral) == "Magnanimous");
    CHECK(captions.CaptionMood(0, DiplomaticStatus_t::Pact) == "Magnanimous");
}

TEST_CASE("Load needs a Vendetta floor a stored friction can reach", "[ui][standing]")
{
    CHECK_THROWS_AS(LoadEdited_("captions_no_floor.json",
                                [](nlohmann::json& j) { j.erase("vendetta_mood_floor"); }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("captions_negative_floor.json",
                                [](nlohmann::json& j) { j["vendetta_mood_floor"] = -1; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("captions_floor_too_high.json",
                                [](nlohmann::json& j) { j["vendetta_mood_floor"] = 21; }),
                    std::runtime_error);
}
