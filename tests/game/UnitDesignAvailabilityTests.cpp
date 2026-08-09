#include "TestHelpers.h"

#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesignAvailability.h"
#include "game/units/UnitSlotRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace ac;

namespace
{

bool ContainsSlotId_(const std::vector<UnitSlotConfig_t>& rSlots, const std::string& rId)
{
    return std::any_of(rSlots.begin(), rSlots.end(),
                       [&](const UnitSlotConfig_t& rSlot) { return rSlot.id == rId; });
}

bool ContainsComponentId_(const std::vector<const UnitComponentConfig_t*>& rComponents,
                          const std::string& rId)
{
    return std::any_of(rComponents.begin(), rComponents.end(),
                       [&](const UnitComponentConfig_t* pConfig) { return pConfig->id == rId; });
}

} // namespace

TEST_CASE("Unit design slots are gated by discovered techs", "[unit][design][availability]")
{
    UnitSlotRegistry slots;
    slots.Load(actest::FixturePath("unit_slots.json"));

    SECTION("undiscovered tech hides the gated slot")
    {
        const auto available = GetAvailableUnitSlots(slots, {});
        CHECK(ContainsSlotId_(available, "chassis"));
        CHECK_FALSE(ContainsSlotId_(available, "gated_ability"));
    }

    SECTION("discovering the tech unlocks the gated slot")
    {
        const auto available = GetAvailableUnitSlots(slots, {"advanced_build"});
        CHECK(ContainsSlotId_(available, "gated_ability"));
    }
}

TEST_CASE("Unit design components are gated by discovered techs", "[unit][design][availability]")
{
    UnitComponentRegistry components;
    components.Load(actest::FixturePath("unit_components.json"));

    SECTION("undiscovered tech hides the gated component")
    {
        const auto available = GetAvailableUnitComponents(components, {});
        CHECK(ContainsComponentId_(available, "test_chassis"));
        CHECK_FALSE(ContainsComponentId_(available, "test_gated_ability"));
    }

    SECTION("discovering the tech unlocks the gated component")
    {
        const auto available = GetAvailableUnitComponents(components, {"advanced_build"});
        CHECK(ContainsComponentId_(available, "test_gated_ability"));
    }

    SECTION("type filter still respects the tech gate")
    {
        const auto locked = GetAvailableUnitComponents(components, "ability", {});
        CHECK_FALSE(ContainsComponentId_(locked, "test_gated_ability"));

        const auto unlocked =
            GetAvailableUnitComponents(components, "ability", {"advanced_build"});
        CHECK(ContainsComponentId_(unlocked, "test_gated_ability"));
        CHECK_FALSE(ContainsComponentId_(unlocked, "test_chassis"));
    }
}
