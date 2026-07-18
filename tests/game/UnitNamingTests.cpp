#include "TestHelpers.h"

#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentConfigParser.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "game/effects/EffectEnums.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
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

TEST_CASE("UnitComponentConfigParser reads unit_name", "[unit][naming][parser]")
{
    UnitComponentConfigParser parser;
    const json j = {
        {"id", "Hand_Weapons"},
        {"name", "Hand Weapons"},
        {"unit_name", "Scout"},
        {"type", "weapon"},
        {"effects", json::array()}
    };

    const UnitComponentConfig_t config = parser.ParseComponentConfig(j);
    CHECK(config.unitName == "Scout");
    CHECK(config.name == "Hand Weapons");
}

TEST_CASE("UnitDesign name is Weapon Armour Chassis with empty parts omitted", "[unit][naming]")
{
    actest::EffectPool pool;

    UnitComponentConfig_t laser;
    laser.id = "Laser";
    laser.type = "weapon";
    laser.unitName = "Laser";

    UnitComponentConfig_t handWeapons;
    handWeapons.id = "Hand_Weapons";
    handWeapons.type = "weapon";
    handWeapons.unitName = "Scout";

    UnitComponentConfig_t plasma;
    plasma.id = "Plasma_Steel_Armour";
    plasma.type = "armour";
    plasma.unitName = "Plasma";

    UnitComponentConfig_t noArmour;
    noArmour.id = "No_Armour";
    noArmour.type = "armour";
    noArmour.unitName = "";

    UnitComponentConfig_t infantry;
    infantry.id = "Infantry";
    infantry.type = "chassis";
    infantry.domain = UnitDomain_t::Land;
    infantry.unitName = "Infantry";
    infantry.effects = {
        pool.StatMod(StatId_t::Movement, 1.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
    };

    UnitComponentConfig_t speeder;
    speeder.id = "Speeder";
    speeder.type = "chassis";
    speeder.domain = UnitDomain_t::Land;
    speeder.unitName = "Speeder";
    speeder.effects = {
        pool.StatMod(StatId_t::Movement, 2.0, ModifierOp_t::Add, EffectScope_t::ThisUnit),
    };

    UnitComponentConfig_t reactor;
    reactor.id = "Fission_Plant";
    reactor.type = "reactor";

    UnitComponentConfig_t ability;
    ability.id = "Deep_Radar";
    ability.type = "ability";
    ability.name = "Deep Radar";

    const std::vector<UnitSlotConfig_t> slots = {
        MakeSlot_("chassis", "chassis"),
        MakeSlot_("weapon", "weapon"),
        MakeSlot_("armour", "armour"),
        MakeSlot_("reactor", "reactor"),
        MakeSlot_("ability_1", "ability", false),
    };

    SECTION("Laser Plasma Infantry")
    {
        const std::unordered_map<std::string, const UnitComponentConfig_t*> components = {
            {"chassis", &infantry},
            {"weapon", &laser},
            {"armour", &plasma},
            {"reactor", &reactor},
        };
        const UnitDesign design(slots, components);
        CHECK(design.GetName() == "Laser Plasma Infantry");
    }

    SECTION("Scout Speeder omits empty armour name")
    {
        const std::unordered_map<std::string, const UnitComponentConfig_t*> components = {
            {"chassis", &speeder},
            {"weapon", &handWeapons},
            {"armour", &noArmour},
            {"reactor", &reactor},
        };
        const UnitDesign design(slots, components);
        CHECK(design.GetName() == "Scout Speeder");
    }

    SECTION("abilities and reactors do not appear in the display name")
    {
        const std::unordered_map<std::string, const UnitComponentConfig_t*> components = {
            {"chassis", &infantry},
            {"weapon", &laser},
            {"armour", &plasma},
            {"reactor", &reactor},
            {"ability_1", &ability},
        };
        const UnitDesign design(slots, components);
        CHECK(design.GetName() == "Laser Plasma Infantry");
        CHECK(design.GetId().find("Deep_Radar") != std::string::npos);
    }
}
