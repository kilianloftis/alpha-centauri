#include "TestHelpers.h"
#include "TestTempDir.h"

#include "ui/text/TextPack.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

TextPack LoadOverlay_()
{
    return TextPack::Load({FixturePath("text_overlay/first"), FixturePath("text_overlay/second")},
                          "en");
}

// A one-root pack whose faction file holds rJson.
TextPack LoadVocabulary_(const std::string& rName, const std::string& rJson)
{
    const std::filesystem::path root = TestTempDir::Path() / rName;
    std::filesystem::create_directories(root / "en" / "factions");
    std::ofstream(root / "en" / "factions" / "x.json") << rJson;
    return TextPack::Load({root.string()}, "en");
}

// The fixture tester's vocabulary, for a test to break.
nlohmann::json TesterVocabulary_()
{
    std::ifstream file(FixturePath("text/en/factions/test_faction.json"));
    return nlohmann::json::parse(file);
}

} // namespace

TEST_CASE("Each script entry and label comes from the first root that has it",
          "[ui][text][pack]")
{
    const TextPack pack = LoadOverlay_();
    CHECK(pack.Entry("SHARED").body == "first shared");
    CHECK(pack.Entry("FIRST_ONLY").body == "first only");
    CHECK(pack.Entry("SECOND_ONLY").body == "second only");
    CHECK(pack.Label(7) == "first seven");
    CHECK(pack.Label(8) == "second eight");
}

TEST_CASE("A faction's vocabulary comes whole from the first root that has it",
          "[ui][text][pack]")
{
    const TextPack pack = LoadOverlay_();
    const FactionVocabulary_t& rShared = pack.Vocabulary("shared");
    CHECK(rShared.fields.at("name_leader").text == "First");
    CHECK(rShared.fields.at("title_leader").text == "Lady");
    CHECK(pack.Vocabulary("second_only").fields.at("name_leader").text == "Only");
}

TEST_CASE("Leader fields carry the leader's sex, faction fields the noun's grammar",
          "[ui][text][pack]")
{
    const TextPack pack = LoadOverlay_();
    const FactionVocabulary_t& rShared = pack.Vocabulary("shared");
    CHECK(rShared.fields.at("title_leader").gender == GrammaticalGender_t::Feminine);
    CHECK_FALSE(rShared.fields.at("title_leader").bPlural);
    CHECK(rShared.fields.at("noun_faction").gender == GrammaticalGender_t::Masculine);
    CHECK(rShared.fields.at("noun_faction").bPlural);
}

TEST_CASE("Entries keep the body and items the extractor split", "[ui][text][pack]")
{
    const TextPack pack = LoadOverlay_();
    const ScriptEntry_t& rEntry = pack.Entry("POPUP");
    CHECK(rEntry.body == "body line");
    CHECK(rEntry.items == std::vector<std::string>{"item one", "item two"});
}

TEST_CASE("A root without the locale is skipped", "[ui][text][pack]")
{
    const TextPack pack =
        TextPack::Load({FixturePath("text_overlay/missing"), FixturePath("text_overlay/second")},
                       "en");
    CHECK(pack.Entry("SHARED").body == "second shared");
}

TEST_CASE("An id no root has throws", "[ui][text][pack]")
{
    const TextPack pack = LoadOverlay_();
    CHECK_THROWS_AS(pack.Entry("NOWHERE"), std::runtime_error);
    CHECK_THROWS_AS(pack.Label(999), std::runtime_error);
    CHECK_THROWS_AS(pack.Vocabulary("nobody"), std::runtime_error);
}

TEST_CASE("A faction vocabulary needs every header field", "[ui][text][pack]")
{
    CHECK_THROWS_AS(LoadVocabulary_("vocab_missing_field",
                                    R"({"leader_female": false, "noun_gender": "M",
                                        "noun_plural": false, "fields": {}, "phrases": {}})"),
                    std::runtime_error);
    CHECK_THROWS_AS(LoadVocabulary_("vocab_bad_gender",
                                    R"({"leader_female": false, "noun_gender": "X",
                                        "noun_plural": false, "fields": {}, "phrases": {}})"),
                    std::runtime_error);
}

TEST_CASE("Phrase lines are named for the sentence each completes", "[ui][text][pack]")
{
    const TextPack pack = TextPack::Load({FixturePath("text")}, "en");
    const FactionVocabulary_t& rTester = pack.Vocabulary("test_faction");
    CHECK(rTester.phrases.size() == 15);
    CHECK(rTester.phrases.at("intent") == "tester intent");
    CHECK(rTester.phrases.at("projects") == "tester projects, M2");
}

TEST_CASE("A faction vocabulary needs exactly the named phrases", "[ui][text][pack]")
{
    nlohmann::json missing = TesterVocabulary_();
    missing["phrases"].erase("code");
    CHECK_THROWS_AS(LoadVocabulary_("vocab_missing_phrase", missing.dump()), std::runtime_error);

    nlohmann::json unknown = TesterVocabulary_();
    unknown["phrases"]["chorus"] = "an extra line";
    CHECK_THROWS_AS(LoadVocabulary_("vocab_unknown_phrase", unknown.dump()), std::runtime_error);

    nlohmann::json listed = TesterVocabulary_();
    listed["phrases"] = nlohmann::json::array({"a line"});
    CHECK_THROWS_AS(LoadVocabulary_("vocab_listed_phrases", listed.dump()), std::runtime_error);
}
