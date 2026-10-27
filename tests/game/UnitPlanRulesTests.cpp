#include "TestHelpers.h"

#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitPlan.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <deque>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

// Parts carry only the values make_proto reads: chassis speed, weapon offense, armour defense,
// a weapon's plan and an ability's override (docs/thinker/terranx-map.md, "Unit plan").
class PlanDesigns_
{
public:
    const UnitComponentConfig_t& Chassis(UnitDomain_t domain, int speed)
    {
        UnitComponentConfig_t& rPart = NewPart_("chassis");
        rPart.domain = domain;
        rPart.effects = {m_pool.StatMod(StatId_t::Movement, speed, ModifierOp_t::Add,
                                        EffectScope_t::ThisUnit)};
        return rPart;
    }

    // Attack 0 stands for a psi weapon, which has no offense value of its own.
    const UnitComponentConfig_t& Weapon(int attack)
    {
        UnitComponentConfig_t& rPart = NewPart_("weapon");
        rPart.effects = {m_pool.StatMod(StatId_t::Attack, attack, ModifierOp_t::Add,
                                        EffectScope_t::ThisUnit)};
        return rPart;
    }

    const UnitComponentConfig_t& PlannedWeapon(UnitPlan_t plan)
    {
        UnitComponentConfig_t& rPart = NewPart_("weapon");
        rPart.plan = plan;
        return rPart;
    }

    const UnitComponentConfig_t& Armour(int defense)
    {
        UnitComponentConfig_t& rPart = NewPart_("armour");
        rPart.effects = {m_pool.StatMod(StatId_t::Defense, defense, ModifierOp_t::Add,
                                        EffectScope_t::ThisUnit)};
        return rPart;
    }

    const UnitComponentConfig_t& Ability(UnitPlan_t plan)
    {
        UnitComponentConfig_t& rPart = NewPart_("ability");
        rPart.plan = plan;
        return rPart;
    }

    UnitPlan_t PlanOf(std::initializer_list<const UnitComponentConfig_t*> parts)
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        for (const UnitComponentConfig_t* pPart : parts)
        {
            slots.push_back({.id = pPart->id, .displayName = pPart->id,
                             .componentType = pPart->type, .required = true});
            assigned[pPart->id] = pPart;
        }
        const UnitDesign design(slots, assigned);
        return design.GetPlan();
    }

private:
    UnitComponentConfig_t& NewPart_(const std::string& rType)
    {
        UnitComponentConfig_t& rPart = m_parts.emplace_back();
        rPart.id = rType + "_" + std::to_string(m_parts.size());
        rPart.type = rType;
        return rPart;
    }

    EffectPool m_pool;
    std::deque<UnitComponentConfig_t> m_parts;
};

} // namespace

TEST_CASE("A weapon's plan wins over the domain and the stats", "[units][plan]")
{
    PlanDesigns_ designs;
    const UnitComponentConfig_t& rInfantry = designs.Chassis(UnitDomain_t::Land, 1);
    const UnitComponentConfig_t& rFoil = designs.Chassis(UnitDomain_t::Sea, 2);
    const UnitComponentConfig_t& rMissile = designs.Chassis(UnitDomain_t::Orbital, 8);
    const UnitComponentConfig_t& rArmour = designs.Armour(3);

    CHECK(designs.PlanOf({&rInfantry, &designs.PlannedWeapon(UnitPlan_t::Colony), &rArmour})
          == UnitPlan_t::Colony);
    CHECK(designs.PlanOf({&rFoil, &designs.PlannedWeapon(UnitPlan_t::Terraform)})
          == UnitPlan_t::Terraform);
    CHECK(designs.PlanOf({&rMissile, &designs.PlannedWeapon(UnitPlan_t::TectonicMissile)})
          == UnitPlan_t::TectonicMissile);
}

TEST_CASE("An armed sea design is naval superiority whatever its stats", "[units][plan]")
{
    PlanDesigns_ designs;
    const UnitComponentConfig_t& rFoil = designs.Chassis(UnitDomain_t::Sea, 4);

    CHECK(designs.PlanOf({&rFoil, &designs.Weapon(1), &designs.Armour(3)})
          == UnitPlan_t::NavalSuperiority);
    CHECK(designs.PlanOf({&rFoil, &designs.Weapon(4), &designs.Armour(1),
                          &designs.Ability(UnitPlan_t::Offense)})
          == UnitPlan_t::NavalSuperiority);
}

TEST_CASE("An armed air design attacks unless it carries Air Superiority", "[units][plan]")
{
    PlanDesigns_ designs;
    const UnitComponentConfig_t& rJet = designs.Chassis(UnitDomain_t::Air, 8);

    CHECK(designs.PlanOf({&rJet, &designs.Weapon(2), &designs.Armour(3)}) == UnitPlan_t::Offense);
    CHECK(designs.PlanOf({&rJet, &designs.Weapon(2), &designs.Armour(1),
                          &designs.Ability(UnitPlan_t::AirSuperiority)})
          == UnitPlan_t::AirSuperiority);
}

TEST_CASE("A missile without a payload plan attacks", "[units][plan]")
{
    PlanDesigns_ designs;
    const UnitComponentConfig_t& rMissile = designs.Chassis(UnitDomain_t::Orbital, 8);

    CHECK(designs.PlanOf({&rMissile, &designs.Weapon(12)}) == UnitPlan_t::Offense);
}

TEST_CASE("Land plans follow armour, then offense against armour, then speed",
          "[units][plan]")
{
    PlanDesigns_ designs;
    const UnitComponentConfig_t& rInfantry = designs.Chassis(UnitDomain_t::Land, 1);
    const UnitComponentConfig_t& rSpeeder = designs.Chassis(UnitDomain_t::Land, 2);

    // No armour beyond the base value: offense.
    CHECK(designs.PlanOf({&rInfantry, &designs.Weapon(4), &designs.Armour(1)})
          == UnitPlan_t::Offense);
    // Armour that outclasses the weapon: defense.
    CHECK(designs.PlanOf({&rInfantry, &designs.Weapon(1), &designs.Armour(3)})
          == UnitPlan_t::Defense);
    // A weapon at least as strong as the armour: combat, compared unscaled.
    CHECK(designs.PlanOf({&rInfantry, &designs.Weapon(3), &designs.Armour(3)})
          == UnitPlan_t::Combat);
    CHECK(designs.PlanOf({&rInfantry, &designs.Weapon(8), &designs.Armour(4)})
          == UnitPlan_t::Combat);
    // Fast and out-gunning its armour: offense.
    CHECK(designs.PlanOf({&rSpeeder, &designs.Weapon(4), &designs.Armour(3)})
          == UnitPlan_t::Offense);
    // Fast but evenly matched stays combat.
    CHECK(designs.PlanOf({&rSpeeder, &designs.Weapon(3), &designs.Armour(3)})
          == UnitPlan_t::Combat);
}

TEST_CASE("A land offense ability makes any armed land design offense", "[units][plan]")
{
    PlanDesigns_ designs;
    const UnitComponentConfig_t& rInfantry = designs.Chassis(UnitDomain_t::Land, 1);

    CHECK(designs.PlanOf({&rInfantry, &designs.Weapon(1), &designs.Armour(3),
                          &designs.Ability(UnitPlan_t::Offense)})
          == UnitPlan_t::Offense);
}

TEST_CASE("A gun with no armour scouts when fast and polices with a defense ability",
          "[units][plan]")
{
    PlanDesigns_ designs;
    const UnitComponentConfig_t& rInfantry = designs.Chassis(UnitDomain_t::Land, 1);
    const UnitComponentConfig_t& rSpeeder = designs.Chassis(UnitDomain_t::Land, 2);

    CHECK(designs.PlanOf({&rInfantry, &designs.Weapon(1), &designs.Armour(1)})
          == UnitPlan_t::Offense);
    CHECK(designs.PlanOf({&rSpeeder, &designs.Weapon(1), &designs.Armour(1)})
          == UnitPlan_t::Recon);
    CHECK(designs.PlanOf({&rSpeeder, &designs.Weapon(1), &designs.Armour(1),
                          &designs.Ability(UnitPlan_t::Defense)})
          == UnitPlan_t::Defense);
    // The defense ability applies to the gun-and-no-armour design only.
    CHECK(designs.PlanOf({&rInfantry, &designs.Weapon(4), &designs.Armour(1),
                          &designs.Ability(UnitPlan_t::Defense)})
          == UnitPlan_t::Offense);
}

TEST_CASE("A psi weapon behind real armour is defense", "[units][plan]")
{
    PlanDesigns_ designs;
    const UnitComponentConfig_t& rInfantry = designs.Chassis(UnitDomain_t::Land, 1);

    CHECK(designs.PlanOf({&rInfantry, &designs.Weapon(0), &designs.Armour(2)})
          == UnitPlan_t::Defense);
}

TEST_CASE("A component plan is read from weapons and abilities only", "[units][plan][parser]")
{
    const auto component = [](const char* pType, const char* pPlan)
    {
        return nlohmann::json{{"id", "part"}, {"name", "Part"}, {"type", pType},
                              {"mineral_cost", 0}, {"plan", pPlan}};
    };

    CHECK(ParseOneComponent(component("weapon", "colony")).plan == UnitPlan_t::Colony);
    CHECK(ParseOneComponent(component("ability", "air_superiority")).plan
          == UnitPlan_t::AirSuperiority);
    // An ability overrides make_proto only to offense, air superiority or defense.
    CHECK_THROWS(ParseOneComponent(component("ability", "colony")));
    CHECK_THROWS(ParseOneComponent(component("armour", "defense")));
    CHECK_THROWS(ParseOneComponent(component("weapon", "skirmish")));

    const nlohmann::json plain = {{"id", "part"}, {"name", "Part"}, {"type", "weapon"},
                                  {"mineral_cost", 0}};
    CHECK_FALSE(ParseOneComponent(plain).plan.has_value());
}
