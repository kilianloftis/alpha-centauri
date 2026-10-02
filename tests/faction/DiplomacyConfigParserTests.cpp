#include "TestHelpers.h"

#include "game/effects/EffectEnums.h"
#include "game/faction/DiplomacyConfigParser.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <variant>

using namespace ac;
using namespace actest;

namespace
{

nlohmann::json LoadFixture_()
{
    std::ifstream in(FixturePath("diplomacy.json"));
    return nlohmann::json::parse(in);
}

DiplomacyConfig_t ParseEdited_(const std::string& rName,
                               const std::function<void(nlohmann::json&)>& rEdit)
{
    nlohmann::json json = LoadFixture_();
    rEdit(json);
    const std::filesystem::path path = std::filesystem::temp_directory_path() / rName;
    std::ofstream(path) << json.dump();
    return DiplomacyConfigParser{}.ParseConfig(path.string());
}

} // namespace

TEST_CASE("diplomacy.json loads every status's rules", "[diplomacy][parser]")
{
    const DiplomacyConfig_t config = DiplomacyConfigParser{}.ParseConfig(FixturePath("diplomacy.json"));

    const DiplomaticStatusRules_t& rTruce = config.For(DiplomaticStatus_t::Truce);
    CHECK(rTruce.bEnterTerritory);
    CHECK(rTruce.durationTurns == 3);

    const DiplomaticStatusRules_t& rTreaty = config.For(DiplomaticStatus_t::Treaty);
    CHECK_FALSE(rTreaty.bEnterTerritory);
    REQUIRE(rTreaty.effects.size() == 1);
    CHECK(rTreaty.effects.front().scope == EffectScope_t::FactionPair);
    const auto* pModifier = std::get_if<StatModifierEffect_t>(&rTreaty.effects.front().effect);
    REQUIRE(pModifier);
    CHECK(pModifier->stat == StatId_t::CommerceRate);
    CHECK(pModifier->op == ModifierOp_t::MultiplyGeometric);
    CHECK(pModifier->amount == 0.5);
    CHECK_FALSE(rTreaty.durationTurns.has_value());

    const DiplomaticStatusRules_t& rPact = config.For(DiplomaticStatus_t::Pact);
    CHECK(rPact.bShareTiles);
    CHECK(rPact.bRepairAtBases);
    CHECK(rPact.bDefensiveObligation);
    CHECK_FALSE(rPact.bMayAttack);

    CHECK(config.For(DiplomaticStatus_t::Vendetta).bMayAttack);
    CHECK_FALSE(config.For(DiplomaticStatus_t::Neutral).bMayAttack);
    CHECK(config.defensiveObligationMode == DefensiveObligationMode_t::JoinAsDefender);
}

TEST_CASE("The diplomacy parser rejects incomplete or invalid statuses", "[diplomacy][parser]")
{
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_missing_status.json",
                                 [](nlohmann::json& j) { j["statuses"].erase("Truce"); }),
                    std::runtime_error);
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_bad_status.json",
                                 [](nlohmann::json& j)
                                 { j["statuses"]["Friendship"] = j["statuses"]["Treaty"]; }),
                    std::runtime_error);
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_missing_key.json",
                                 [](nlohmann::json& j)
                                 { j["statuses"]["Pact"].erase("share_tiles"); }),
                    std::runtime_error);
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_unknown_key.json",
                                 [](nlohmann::json& j)
                                 { j["statuses"]["Pact"]["broken_to"] = "Treaty"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_zero_duration.json",
                                 [](nlohmann::json& j)
                                 { j["statuses"]["Truce"]["duration_turns"] = 0; }),
                    std::runtime_error);
}

TEST_CASE("Status effects are limited to unconditional FactionPair commerce_rate modifiers",
          "[diplomacy][parser]")
{
    const auto withTreatyEffect = [](const std::string& rName, const char* pField,
                                     const nlohmann::json& rValue)
    {
        return ParseEdited_(rName,
                            [&](nlohmann::json& j)
                            {
                                nlohmann::json& rEffect = j["statuses"]["Treaty"]["effects"][0];
                                if (std::string(pField) == "stat")
                                {
                                    rEffect["parameters"]["stat"] = rValue;
                                }
                                else
                                {
                                    rEffect[pField] = rValue;
                                }
                            });
    };
    CHECK_THROWS_AS(withTreatyEffect("ac_diplomacy_other_stat.json", "stat", "energy"),
                    std::runtime_error);
    CHECK_THROWS_AS(withTreatyEffect("ac_diplomacy_world_scope.json", "scope", "WorldGlobal"),
                    std::runtime_error);
    CHECK_THROWS_AS(withTreatyEffect("ac_diplomacy_global_scope.json", "scope", "FactionGlobal"),
                    std::runtime_error);
}

TEST_CASE("A duration is only legal on a status that can step down", "[diplomacy][parser]")
{
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_vendetta_duration.json",
                                 [](nlohmann::json& j)
                                 { j["statuses"]["Vendetta"]["duration_turns"] = 5; }),
                    std::runtime_error);
    CHECK(ParseEdited_("ac_diplomacy_pact_duration.json",
                       [](nlohmann::json& j) { j["statuses"]["Pact"]["duration_turns"] = 5; })
              .For(DiplomaticStatus_t::Pact)
              .durationTurns
          == 5);
}

TEST_CASE("The diplomacy parser requires a known defensive obligation mode", "[diplomacy][parser]")
{
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_missing_mode.json",
                                 [](nlohmann::json& j) { j.erase("defensive_obligation_mode"); }),
                    std::runtime_error);
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_unknown_mode.json",
                                 [](nlohmann::json& j)
                                 { j["defensive_obligation_mode"] = "StandAside"; }),
                    std::runtime_error);
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_numeric_mode.json",
                                 [](nlohmann::json& j) { j["defensive_obligation_mode"] = 1; }),
                    std::runtime_error);
    CHECK(ParseEdited_("ac_diplomacy_separate_mode.json",
                       [](nlohmann::json& j)
                       { j["defensive_obligation_mode"] = "SeparateDeclaration"; })
              .defensiveObligationMode
          == DefensiveObligationMode_t::SeparateDeclaration);
}

TEST_CASE("A defensive obligation is only legal on a status that can step down",
          "[diplomacy][parser]")
{
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_neutral_obligation.json",
                                 [](nlohmann::json& j)
                                 { j["statuses"]["Neutral"]["defensive_obligation"] = true; }),
                    std::runtime_error);
    CHECK_THROWS_AS(ParseEdited_("ac_diplomacy_vendetta_obligation.json",
                                 [](nlohmann::json& j)
                                 { j["statuses"]["Vendetta"]["defensive_obligation"] = true; }),
                    std::runtime_error);
    CHECK(ParseEdited_("ac_diplomacy_treaty_obligation.json",
                       [](nlohmann::json& j)
                       { j["statuses"]["Treaty"]["defensive_obligation"] = true; })
              .For(DiplomaticStatus_t::Treaty)
              .bDefensiveObligation);
}
