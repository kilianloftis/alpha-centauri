#include "TestHelpers.h"
#include "TestTempDir.h"

#include "game/faction/FactionRegistry.h"
#include "ui/commlinks/TalkScript.h"
#include "ui/text/TextPack.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

using namespace ac;
using namespace actest;

namespace
{

const TextPack& FixtureText_()
{
    static const TextPack pack = TextPack::Load({FixturePath("text")}, "en");
    return pack;
}

const FactionRegistry& FixtureFactions_()
{
    static const std::unique_ptr<FactionRegistry> pRegistry = []
    {
        auto pLoaded = std::make_unique<FactionRegistry>();
        pLoaded->Load(FixturePath("factions"));
        return pLoaded;
    }();
    return *pRegistry;
}

nlohmann::json FixtureJson_()
{
    std::ifstream in(FixturePath("ui/talk.json"));
    return nlohmann::json::parse(in);
}

TalkScript LoadEdited_(const std::string& rName, const std::function<void(nlohmann::json&)>& rEdit)
{
    nlohmann::json json = FixtureJson_();
    rEdit(json);
    const std::filesystem::path path = TestTempDir::Path() / (rName + ".json");
    std::ofstream(path) << json.dump();
    return TalkScript::Load(path.string(), FixtureText_(), FixtureFactions_());
}

nlohmann::json& FirstChoice_(nlohmann::json& rJson, const std::string& rScene)
{
    return rJson["scenes"][rScene]["choices"][0];
}

} // namespace

TEST_CASE("The talk script loads its portrait, offers, start and scenes", "[ui][talk][script]")
{
    const TalkScript script =
        TalkScript::Load(FixturePath("ui/talk.json"), FixtureText_(), FixtureFactions_());

    CHECK(script.Portrait().mood == 933);
    CHECK(script.Portrait().reputation == 932);
    CHECK(script.Portrait().integrity == 222);
    CHECK(script.CreditOffers() == std::vector<int>{10, 50});
    REQUIRE(script.Start().say.size() == 1);
    CHECK(script.Start().say.front().id == "GREET");
    CHECK(script.Start().then == "diplomacy");
    CHECK(script.Scene("proposal").choices.size() == 5);
    CHECK_THROWS_AS(script.Scene("nowhere"), std::runtime_error);
}

TEST_CASE("A choice with then sends every result of its action there", "[ui][talk][script]")
{
    const TalkScript script =
        TalkScript::Load(FixturePath("ui/talk.json"), FixtureText_(), FixtureFactions_());
    const auto& rRenounce = std::get<TalkFixedChoice_t>(script.Scene("confirm_renounce").choices[1]);
    REQUIRE(rRenounce.action == TalkAction_t::RenouncePact);
    REQUIRE(rRenounce.outcomes.size() == 1);
    CHECK(rRenounce.outcomes.at(TalkResult_t::Done).then == "diplomacy");
}

TEST_CASE("The talk script rejects malformed flow", "[ui][talk][script]")
{
    CHECK_THROWS_AS(LoadEdited_("talk_unknown_key", [](nlohmann::json& j) { j["extra"] = 1; }),
                    std::runtime_error);
    CHECK_THROWS(LoadEdited_("talk_no_start", [](nlohmann::json& j) { j.erase("start"); }));
    CHECK_THROWS_AS(LoadEdited_("talk_bad_then",
                                [](nlohmann::json& j) { j["start"]["then"] = "nowhere"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_no_offers",
                                [](nlohmann::json& j) { j["credit_offers"] = nlohmann::json::array(); }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_both_then_and_on",
                                [](nlohmann::json& j)
                                { FirstChoice_(j, "proposal")["then"] = "diplomacy"; }),
                    std::runtime_error);
}

TEST_CASE("The talk script rejects text the pack lacks", "[ui][talk][script]")
{
    CHECK_THROWS(LoadEdited_("talk_unknown_text",
                             [](nlohmann::json& j) { j["scenes"]["diplomacy"]["prompt"]["text"] = "NOPE"; }));
    CHECK_THROWS_AS(LoadEdited_("talk_item_past_end",
                                [](nlohmann::json& j)
                                { FirstChoice_(j, "diplomacy")["label"]["item"] = 9; }),
                    std::runtime_error);
    CHECK_THROWS(LoadEdited_("talk_unknown_label",
                             [](nlohmann::json& j) { j["portrait"]["mood"] = 999; }));
}

TEST_CASE("The talk script rejects names outside its vocabularies", "[ui][talk][script]")
{
    CHECK_THROWS_AS(LoadEdited_("talk_unknown_action",
                                [](nlohmann::json& j) { FirstChoice_(j, "proposal")["do"] = "Sing"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_unknown_condition",
                                [](nlohmann::json& j)
                                { j["scenes"]["diplomacy"]["choices"][1]["when"] = "Always"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_unknown_generator",
                                [](nlohmann::json& j) { FirstChoice_(j, "trade")["from"] = "all"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_unknown_binding",
                                [](nlohmann::json& j) { j["start"]["say"][0]["args"] = "nobody"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_unknown_source",
                                [](nlohmann::json& j) { j["bindings"]["greet"]["0"] = "moon.phase"; }),
                    std::runtime_error);
}

TEST_CASE("The talk script rejects unbound and mistyped slots", "[ui][talk][script]")
{
    CHECK_THROWS_AS(LoadEdited_("talk_unbound_slot",
                                [](nlohmann::json& j) { j["bindings"]["greet"].erase("4"); }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_number_from_string",
                                [](nlohmann::json& j) { j["bindings"]["credits"]["#0"] = "player.name_leader"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_candidate_outside_rows",
                                [](nlohmann::json& j) { j["bindings"]["greet"]["5"] = "candidate.name"; }),
                    std::runtime_error);
}

TEST_CASE("The talk script needs every result and every trade kind covered", "[ui][talk][script]")
{
    CHECK_THROWS_AS(LoadEdited_("talk_result_missing",
                                [](nlohmann::json& j) { FirstChoice_(j, "proposal")["on"].erase("Rejected"); }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_result_unreturned",
                                [](nlohmann::json& j)
                                { FirstChoice_(j, "proposal")["on"]["Done"] = {{"then", "diplomacy"}}; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_kind_unlabelled",
                                [](nlohmann::json& j) { FirstChoice_(j, "trade")["labels"].erase("Base"); }),
                    std::runtime_error);
}

TEST_CASE("Every faction that talks must have the words the bindings read", "[ui][talk][script]")
{
    CHECK_THROWS_AS(LoadEdited_("talk_missing_field",
                                [](nlohmann::json& j) { j["bindings"]["greet"]["4"] = "rival.no_such_field"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadEdited_("talk_missing_phrase",
                                [](nlohmann::json& j)
                                {
                                    j["bindings"]["greet"]["5"] = "rival.phrase.chorus";
                                }),
                    std::runtime_error);

    const std::filesystem::path factions = TestTempDir::Path() / "talk_unvoiced_factions";
    const std::filesystem::path dir = factions / "unvoiced";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "identity.json") << R"({"name": "Unvoiced"})";
    std::ofstream(dir / "leader.json") << R"({"name": "X"})";
    std::ofstream(dir / "ai.json") << R"({})";
    std::ofstream(dir / "effects.json") << R"({"effects": []})";
    std::ofstream(dir / "base_names.json") << R"([])";
    FactionRegistry unvoiced;
    unvoiced.Load(factions.string());
    CHECK_THROWS_AS(TalkScript::Load(FixturePath("ui/talk.json"), FixtureText_(), unvoiced),
                    std::runtime_error);
}
