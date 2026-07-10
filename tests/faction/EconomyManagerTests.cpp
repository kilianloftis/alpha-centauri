#include "game/faction/EconomyManager.h"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

using namespace ac;

TEST_CASE("Energy allocation conserves total energy", "[economy]")
{
    EconomyManager economy;

    SECTION("default 40/50/10 split on 5 energy (the 3.1 counterexample)")
    {
        // Independent rounding used to mint a 6th energy: round(2)+round(2.5)+round(0.5).
        // Residual-economy rule: labs=2, psych=0, econ=3.
        CHECK(economy.CalculateEnergyForLabs(5) == 2);
        CHECK(economy.CalculateEnergyForPsych(5) == 0);
        CHECK(economy.CalculateEnergyForEcon(5) == 3);
        CHECK(economy.CalculateEnergyForEcon(5)
            + economy.CalculateEnergyForLabs(5)
            + economy.CalculateEnergyForPsych(5) == 5);
    }

    SECTION("exact multiples conserve without remainder")
    {
        CHECK(economy.CalculateEnergyForEcon(10) == 4);
        CHECK(economy.CalculateEnergyForLabs(10) == 5);
        CHECK(economy.CalculateEnergyForPsych(10) == 1);
    }

    SECTION("custom split still conserves")
    {
        economy.SetEnergyAllocation({30, 60, 10});
        const int total = 7;
        CHECK(economy.CalculateEnergyForEcon(total)
            + economy.CalculateEnergyForLabs(total)
            + economy.CalculateEnergyForPsych(total) == total);
    }

    SECTION("zero energy yields zero everywhere")
    {
        CHECK(economy.CalculateEnergyForEcon(0) == 0);
        CHECK(economy.CalculateEnergyForLabs(0) == 0);
        CHECK(economy.CalculateEnergyForPsych(0) == 0);
    }
}

TEST_CASE("SetEnergyAllocation rejects percentages that do not sum to 100", "[economy]")
{
    EconomyManager economy;
    CHECK_THROWS_AS(economy.SetEnergyAllocation({40, 50, 20}), std::runtime_error);
    CHECK_THROWS_AS(economy.SetEnergyAllocation({40, 50, 0}), std::runtime_error);

    // Prior allocation is unchanged after a rejected set.
    CHECK(economy.GetEnergyAllocation().econPercent == 40);
    CHECK(economy.GetEnergyAllocation().labsPercent == 50);
    CHECK(economy.GetEnergyAllocation().psychPercent == 10);
}
