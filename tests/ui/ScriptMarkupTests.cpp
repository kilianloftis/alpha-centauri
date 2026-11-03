#include "ui/text/ScriptMarkup.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;

namespace
{

ScriptValue_t Masculine_(std::string text)
{
    return ScriptValue_t{std::move(text), GrammaticalGender_t::Masculine, false};
}

ScriptValue_t Feminine_(std::string text)
{
    return ScriptValue_t{std::move(text), GrammaticalGender_t::Feminine, false};
}

} // namespace

TEST_CASE("A $WORDn slot reads the first digit after the $", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetString(0, Masculine_("Lady"));
    args.SetString(1, Masculine_("Deirdre"));

    CHECK(FormatScript("$TITLE0 $NAME1 speaks.", args) == "Lady Deirdre speaks.");
    // The word is only a hint, so anything up to the digit is skipped.
    CHECK(FormatScript("[$ANY_WORD 1]", args) == "[Deirdre]");
}

TEST_CASE("Literal dollars and dollars without a digit", "[ui][text][markup]")
{
    const ScriptArgs args;
    CHECK(FormatScript("costs $$5", args) == "costs $5");
    CHECK(FormatScript("no slot: $NONE here", args) == "no slot: $NONE here");
    CHECK(FormatScript("see $LINK<x> now", args) == "see $LINK<x> now");
}

TEST_CASE("Number slots print in decimal", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetNumber(0, 250);
    args.SetNumber(3, 7);
    CHECK(FormatScript("$NUM0 credits, $NUMBER3 turns, $HEX3", args)
          == "250 credits, 7 turns, 7");
}

TEST_CASE("Two forms follow the slot's number", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetString(4, ScriptValue_t{"Gaians", GrammaticalGender_t::Feminine, true});
    args.SetString(5, Masculine_("Hive"));

    CHECK(FormatScript("$<4:its:their>", args) == "their");
    CHECK(FormatScript("$<5:its:their>", args) == "its");
}

TEST_CASE("Three forms follow the slot's gender", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetString(1, Feminine_("Deirdre"));
    args.SetString(2, ScriptValue_t{"it", GrammaticalGender_t::Neuter, false});
    CHECK(FormatScript("$<1:he:she:it>", args) == "she");
    CHECK(FormatScript("$<2:he:she:it>", args) == "it");
}

TEST_CASE("Four forms split masculine from the rest, then by number", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetString(0, Masculine_("Yang"));
    args.SetString(1, Feminine_("Deirdre"));
    args.SetString(2, ScriptValue_t{"they", GrammaticalGender_t::Neuter, true});
    CHECK(FormatScript("$<0:his:her:mp:fp>", args) == "his");
    CHECK(FormatScript("$<1:his:her:mp:fp>", args) == "her");
    CHECK(FormatScript("$<2:his:her:mp:fp>", args) == "fp");
}

TEST_CASE("Six forms split by gender then number", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetString(0, ScriptValue_t{"x", GrammaticalGender_t::Neuter, true});
    CHECK(FormatScript("$<0:ms:fs:ns:mp:fp:np>", args) == "np");
}

TEST_CASE("Any other form count takes the last form", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetString(0, Masculine_("x"));
    CHECK(FormatScript("$<0:a:b:c:d:e>", args) == "e");
}

TEST_CASE("An explicit spec sets gender and number", "[ui][text][markup]")
{
    const ScriptArgs args;
    CHECK(FormatScript("$<F1:his:her:mp:fp>", args) == "her");
    CHECK(FormatScript("$<M2:his:her:mp:fp>", args) == "mp");
    CHECK(FormatScript("$<N:he:she:it>", args) == "it");
}

TEST_CASE("A #n spec is plural unless the number slot is one", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetNumber(0, 1);
    args.SetNumber(1, 3);
    CHECK(FormatScript("$NUM0 $<#0:unit:units>, $NUM1 $<#1:unit:units>", args)
          == "1 unit, 3 units");
}

TEST_CASE("A form with no colons inflects the named slot's own text", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetString(0, Masculine_("Spartan:Spartans"));
    CHECK(FormatScript("$<M1:$FACTIONADJ0>", args) == "Spartan");
    CHECK(FormatScript("$<M2:$FACTIONADJ0>", args) == "Spartans");
}

TEST_CASE("A $< with no closing > drops the $", "[ui][text][markup]")
{
    const ScriptArgs args;
    CHECK(FormatScript("a $<1:b", args) == "a <1:b");
}

TEST_CASE("Malformed slots throw", "[ui][text][markup]")
{
    ScriptArgs args;
    args.SetString(0, Masculine_("x"));
    CHECK_THROWS_AS(FormatScript("$NUMx", args), std::runtime_error);
    CHECK_THROWS_AS(FormatScript("$<Q:a:b>", args), std::runtime_error);
    CHECK_THROWS_AS(FormatScript("$NAME5", args), std::runtime_error);
}

TEST_CASE("SlotsOf lists every slot a template reads", "[ui][text][markup]")
{
    const ScriptSlots_t slots = SlotsOf("$TITLE0 $<4:its:their> $NUM2 $<#3:a:b> $<M1:$ADJ6> $$9");
    CHECK(slots.strings == std::set<int>{0, 4, 6});
    CHECK(slots.numbers == std::set<int>{2, 3});
}

TEST_CASE("Braces mark highlighted runs", "[ui][text][markup]")
{
    const std::vector<TextRun_t> runs = HighlightRuns("pay {50 credits} now");
    REQUIRE(runs.size() == 3);
    CHECK(runs[0].text == "pay ");
    CHECK_FALSE(runs[0].bHighlight);
    CHECK(runs[1].text == "50 credits");
    CHECK(runs[1].bHighlight);
    CHECK(runs[2].text == " now");
    CHECK(WithoutHighlights("pay {50 credits} now") == "pay 50 credits now");
}

TEST_CASE("A noun phrase takes its gender and number from its tag", "[ui][text][markup]")
{
    const ScriptValue_t plural = NounPhrase("environmental initiatives, M2");
    CHECK(plural.text == "environmental initiatives");
    CHECK(plural.gender == GrammaticalGender_t::Masculine);
    CHECK(plural.bPlural);

    const ScriptValue_t feminine = NounPhrase("grant committee, F1");
    CHECK(feminine.gender == GrammaticalGender_t::Feminine);
    CHECK_FALSE(feminine.bPlural);

    const ScriptValue_t untagged = NounPhrase("plain words");
    CHECK(untagged.text == "plain words");
    CHECK(untagged.gender == GrammaticalGender_t::Masculine);
    CHECK_FALSE(untagged.bPlural);
}
