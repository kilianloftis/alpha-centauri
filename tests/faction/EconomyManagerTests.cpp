#include "game/economy/EconomyManager.h"
#include "game/economy/EconomyRules.h"

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
        const EnergySplit_t split = SplitEnergy(economy.GetEnergyAllocation(), 5);
        CHECK(split.labs == 2);
        CHECK(split.psych == 0);
        CHECK(split.econ == 3);
        CHECK(split.econ + split.labs + split.psych == 5);
    }

    SECTION("exact multiples conserve without remainder")
    {
        const EnergySplit_t split = SplitEnergy(economy.GetEnergyAllocation(), 10);
        CHECK(split.econ == 4);
        CHECK(split.labs == 5);
        CHECK(split.psych == 1);
    }

    SECTION("custom split still conserves")
    {
        economy.SetEnergyAllocation({30, 60, 10});
        const int total = 7;
        const EnergySplit_t split = SplitEnergy(economy.GetEnergyAllocation(), total);
        CHECK(split.econ + split.labs + split.psych == total);
    }

    SECTION("zero energy yields zero everywhere")
    {
        const EnergySplit_t split = SplitEnergy(economy.GetEnergyAllocation(), 0);
        CHECK(split.econ == 0);
        CHECK(split.labs == 0);
        CHECK(split.psych == 0);
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

TEST_CASE("EconomyManager owns the never-negative rule", "[faction][economy]")
{
    EconomyManager economy;
    economy.AddEnergy(40);

    CHECK(economy.CanAfford(40));
    CHECK_FALSE(economy.CanAfford(41));
    CHECK_THROWS_AS(economy.CanAfford(-1), std::invalid_argument);

    economy.SpendEnergy(40);
    CHECK(economy.GetEnergy() == 0);
    CHECK_THROWS_AS(economy.SpendEnergy(1), std::runtime_error);
    CHECK(economy.GetEnergy() == 0);
}
