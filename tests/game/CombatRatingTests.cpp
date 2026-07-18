#include "TestHelpers.h"

#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentConfigParser.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "game/effects/EffectEnums.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_map>
#include <vector>

using namespace ac;
using nlohmann::json;

namespace
{

UnitSlotConfig_t MakeSlot_(std::string id, std::string componentType, bool required = true)
{
    UnitSlotConfig_t slot;
    slot.id = std::move(id);
    slot.displayName = slot.id;
    slot.componentType = std::move(componentType);
    slot.required = required;
    return slot;
}

} // namespace

TEST_CASE("UnitComponentConfigParser reads combat rating modifiers and labels",
          "[unit][combat-rating][parser]")
{
    UnitComponentConfigParser parser;
    const json j = {
        {"id", "AAA_Tracking"},
        {"name", "AAA Tracking"},
        {"type", "ability"},
        {"effects", json::array()},
        {"combat_rating_modifiers", json::array({
            {{"target", "defense"}, {"prefix", "<"}, {"suffix", ">"}}
        })},
        {"combat_rating_labels", json::array({"ECM"})}
    };

    const UnitComponentConfig_t config = parser.ParseComponentConfig(j);
    REQUIRE(config.combatRatingModifiers.size() == 1);
    CHECK(config.combatRatingModifiers[0].target == CombatRatingTarget_t::Defense);
    CHECK(config.combatRatingModifiers[0].prefix == "<");
    CHECK(config.combatRatingModifiers[0].suffix == ">");
    REQUIRE(config.combatRatingLabels.size() == 1);
    CHECK(config.combatRatingLabels[0] == "ECM");
}

TEST_CASE("UnitComponentConfigParser rejects invalid combat rating annotations",
          "[unit][combat-rating][parser]")
{
    UnitComponentConfigParser parser;

    SECTION("unknown target")
    {
        const json j = {
            {"id", "bad"},
            {"name", "Bad"},
            {"type", "ability"},
            {"effects", json::array()},
            {"combat_rating_modifiers", json::array({
                {{"target", "hp"}, {"suffix", "!"}}
            })}
        };
        CHECK_THROWS_AS(parser.ParseComponentConfig(j), std::runtime_error);
    }

    SECTION("empty modifier")
    {
        const json j = {
            {"id", "bad"},
            {"name", "Bad"},
            {"type", "ability"},
            {"effects", json::array()},
            {"combat_rating_modifiers", json::array({
                {{"target", "attack"}}
            })}
        };
        CHECK_THROWS_AS(parser.ParseComponentConfig(j), std::runtime_error);
    }

    SECTION("empty label")
    {
        const json j = {
            {"id", "bad"},
            {"name", "Bad"},
            {"type", "ability"},
            {"effects", json::array()},
            {"combat_rating_labels", json::array({""})}
        };
        CHECK_THROWS_AS(parser.ParseComponentConfig(j), std::runtime_error);
    }
}

TEST_CASE("FormatCombatRating applies stacked modifiers, reactor suffix, and labels",
          "[unit][combat-rating]")
{
    actest::EffectPool pool;

    UnitComponentConfig_t weapon;
    weapon.id = "laser";
    weapon.type = "weapon";
    weapon.effects = {
        pool.StatMod(StatId_t::Attack, 2.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
        pool.StatMod(StatId_t::Attack, 50.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit),
    };

    UnitComponentConfig_t armour;
    armour.id = "plasma_resonance";
    armour.type = "armour";
    armour.effects = {
        pool.StatMod(StatId_t::Defense, 3.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
    };
    armour.combatRatingModifiers = {
        {CombatRatingTarget_t::Defense, "", "r"},
    };

    UnitComponentConfig_t chassis;
    chassis.id = "infantry";
    chassis.type = "chassis";
    chassis.domain = UnitDomain_t::Land;
    chassis.effects = {
        pool.StatMod(StatId_t::Movement, 1.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
    };

    UnitComponentConfig_t reactor;
    reactor.id = "fusion";
    reactor.type = "reactor";
    reactor.combatRatingModifiers = {
        {CombatRatingTarget_t::Rating, "", "*2"},
    };

    UnitComponentConfig_t marine;
    marine.id = "marine";
    marine.type = "ability";
    marine.combatRatingModifiers = {
        {CombatRatingTarget_t::Attack, "", "~"},
    };

    UnitComponentConfig_t aaa;
    aaa.id = "aaa";
    aaa.type = "ability";
    aaa.combatRatingModifiers = {
        {CombatRatingTarget_t::Defense, "<", ">"},
    };

    UnitComponentConfig_t ecm;
    ecm.id = "ecm";
    ecm.type = "ability";
    ecm.combatRatingLabels = {"ECM"};

    const std::vector<UnitSlotConfig_t> slots = {
        MakeSlot_("chassis", "chassis"),
        MakeSlot_("weapon", "weapon"),
        MakeSlot_("armour", "armour"),
        MakeSlot_("reactor", "reactor"),
        MakeSlot_("ability_1", "ability", false),
        MakeSlot_("ability_2", "ability", false),
    };

    SECTION("plain additive rating ignores percent modifiers")
    {
        UnitComponentConfig_t plainArmour = armour;
        plainArmour.combatRatingModifiers.clear();
        UnitComponentConfig_t plainReactor = reactor;
        plainReactor.combatRatingModifiers.clear();
        const std::unordered_map<std::string, const UnitComponentConfig_t*> plain = {
            {"chassis", &chassis},
            {"weapon", &weapon},
            {"armour", &plainArmour},
            {"reactor", &plainReactor},
        };
        CHECK(UnitDesign(slots, plain).FormatCombatRating() == "2-3-1");
    }

    SECTION("plasma resonance suffix")
    {
        UnitComponentConfig_t plainReactor = reactor;
        plainReactor.combatRatingModifiers.clear();
        const std::unordered_map<std::string, const UnitComponentConfig_t*> components = {
            {"chassis", &chassis},
            {"weapon", &weapon},
            {"armour", &armour},
            {"reactor", &plainReactor},
        };
        CHECK(UnitDesign(slots, components).FormatCombatRating() == "2-3r-1");
    }

    SECTION("AAA wraps plasma resonance")
    {
        UnitComponentConfig_t plainReactor = reactor;
        plainReactor.combatRatingModifiers.clear();
        const std::unordered_map<std::string, const UnitComponentConfig_t*> components = {
            {"chassis", &chassis},
            {"weapon", &weapon},
            {"armour", &armour},
            {"reactor", &plainReactor},
            {"ability_1", &aaa},
        };
        CHECK(UnitDesign(slots, components).FormatCombatRating() == "2-<3r>-1");
    }

    SECTION("marine, reactor, and ECM compose")
    {
        const std::unordered_map<std::string, const UnitComponentConfig_t*> components = {
            {"chassis", &chassis},
            {"weapon", &weapon},
            {"armour", &armour},
            {"reactor", &reactor},
            {"ability_1", &marine},
            {"ability_2", &ecm},
        };
        CHECK(UnitDesign(slots, components).FormatCombatRating() == "2~-3r-1*2, ECM");
    }

    SECTION("full stack with AAA wrapping resonance and ECM after reactor")
    {
        const std::unordered_map<std::string, const UnitComponentConfig_t*> components = {
            {"chassis", &chassis},
            {"weapon", &weapon},
            {"armour", &armour},
            {"reactor", &reactor},
            {"ability_1", &aaa},
            {"ability_2", &ecm},
        };
        CHECK(UnitDesign(slots, components).FormatCombatRating() == "2-<3r>-1*2, ECM");
    }
}
