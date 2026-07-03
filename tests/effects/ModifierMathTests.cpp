// Tests for the modifier-combination math: ApplyModifierStack and ResolveStatModifiers.
//
// Documented contract (docs/architecture/effects-system.md):
//   total = (baseValue + sum of Add amounts)
//         * (1 + sum of AddPercent amounts / 100)   <- one arithmetic factor, percent points
//         * (product of MultiplyGeometric factors)

#include "TestHelpers.h"

#include "lib/effects/ActiveEffect.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::Active;
using Catch::Approx;

namespace
{
std::vector<std::pair<double, ModifierOp>> Stack(std::initializer_list<std::pair<double, ModifierOp>> init)
{
    return {init};
}
} // namespace

TEST_CASE("ApplyModifierStack: empty stack returns the base value", "[effects][math]")
{
    CHECK(ApplyModifierStack(0.0, {}) == Approx(0.0));
    CHECK(ApplyModifierStack(5.0, {}) == Approx(5.0));
    CHECK(ApplyModifierStack(-3.5, {}) == Approx(-3.5));
}

TEST_CASE("ApplyModifierStack: Add contributions sum onto the base", "[effects][math]")
{
    CHECK(ApplyModifierStack(0.0, Stack({{2.0, ModifierOp::Add}})) == Approx(2.0));
    CHECK(ApplyModifierStack(1.0, Stack({{2.0, ModifierOp::Add}, {3.0, ModifierOp::Add}})) == Approx(6.0));
    CHECK(ApplyModifierStack(4.0, Stack({{-1.0, ModifierOp::Add}})) == Approx(3.0));
}

TEST_CASE("ApplyModifierStack: AddPercent is percent points on top of 100%", "[effects][math]")
{
    // 25 means +25%, matching the UI display.
    CHECK(ApplyModifierStack(4.0, Stack({{25.0, ModifierOp::AddPercent}})) == Approx(5.0));
    CHECK(ApplyModifierStack(4.0, Stack({{-25.0, ModifierOp::AddPercent}})) == Approx(3.0));
}

TEST_CASE("ApplyModifierStack: multiple AddPercent contributions combine arithmetically, not geometrically",
          "[effects][math]")
{
    // +25% and +25% must be x1.5 (1 + 0.25 + 0.25), NOT x1.5625 (1.25 * 1.25).
    CHECK(ApplyModifierStack(4.0, Stack({{25.0, ModifierOp::AddPercent}, {25.0, ModifierOp::AddPercent}}))
          == Approx(6.0));
    // +50% and -50% cancel exactly.
    CHECK(ApplyModifierStack(8.0, Stack({{50.0, ModifierOp::AddPercent}, {-50.0, ModifierOp::AddPercent}}))
          == Approx(8.0));
}

TEST_CASE("ApplyModifierStack: -100% floors the arithmetic factor at zero output", "[effects][math]")
{
    CHECK(ApplyModifierStack(10.0, Stack({{-100.0, ModifierOp::AddPercent}})) == Approx(0.0));
}

TEST_CASE("ApplyModifierStack: MultiplyGeometric factors multiply the running total", "[effects][math]")
{
    // Factor form: 0.5 halves.
    CHECK(ApplyModifierStack(8.0, Stack({{0.5, ModifierOp::MultiplyGeometric}})) == Approx(4.0));
    CHECK(ApplyModifierStack(8.0, Stack({{0.5, ModifierOp::MultiplyGeometric}, {0.5, ModifierOp::MultiplyGeometric}}))
          == Approx(2.0));
    CHECK(ApplyModifierStack(3.0, Stack({{2.0, ModifierOp::MultiplyGeometric}})) == Approx(6.0));
}

TEST_CASE("ApplyModifierStack: combined ops follow (base+adds) * arithmetic * geometric", "[effects][math]")
{
    // (2 + 4) * (1 + 0.5) * 0.5 = 4.5
    const auto stack = Stack({{4.0, ModifierOp::Add},
                              {50.0, ModifierOp::AddPercent},
                              {0.5, ModifierOp::MultiplyGeometric}});
    CHECK(ApplyModifierStack(2.0, stack) == Approx(4.5));
}

TEST_CASE("ApplyModifierStack: result does not depend on contribution order", "[effects][math]")
{
    // An Add listed after a percent/geometric contribution still lands in the additive pool.
    const auto ordered  = Stack({{4.0, ModifierOp::Add},
                                 {50.0, ModifierOp::AddPercent},
                                 {0.5, ModifierOp::MultiplyGeometric}});
    const auto reversed = Stack({{0.5, ModifierOp::MultiplyGeometric},
                                 {50.0, ModifierOp::AddPercent},
                                 {4.0, ModifierOp::Add}});
    CHECK(ApplyModifierStack(2.0, ordered) == Approx(ApplyModifierStack(2.0, reversed)));
}

TEST_CASE("ResolveStatModifiers: empty input resolves to the base value", "[effects][math]")
{
    CHECK(ResolveStatModifiers({}).total == Approx(0.0));
    CHECK(ResolveStatModifiers({}, 7.0).total == Approx(7.0));
    CHECK(ResolveStatModifiers({}).contributions.empty());
}

TEST_CASE("ResolveStatModifiers: sums Add contributions from active effects", "[effects][math]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Nutrients, 2.0), "building_a"),
        Active(pool.StatMod(StatId::Nutrients, 3.0), "building_b"),
    };

    const StatBreakdown_t breakdown = ResolveStatModifiers(effects);
    CHECK(breakdown.total == Approx(5.0));
    REQUIRE(breakdown.contributions.size() == 2);
}

TEST_CASE("ResolveStatModifiers: pure-multiplier stats resolve to zero without a seeded base (known gap)",
          "[effects][math]")
{
    // Documented gap: a stat that only ever receives AddPercent/MultiplyGeometric contributions
    // MUST be resolved with baseValue = 1.0 by the caller (as UnitDesign::GetBaseCost does for
    // CostMultiplier). With the default base of 0.0 the total collapses to 0. This test pins
    // that footgun so any change to it is deliberate.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::CostMultiplier, 1.25, ModifierOp::MultiplyGeometric), "component"),
    };

    CHECK(ResolveStatModifiers(effects).total == Approx(0.0));
    CHECK(ResolveStatModifiers(effects, 1.0).total == Approx(1.25));
}

TEST_CASE("ResolveStatModifiers: seeded base value participates in percent scaling", "[effects][math]")
{
    // GrowthRate-style usage: base 100 (percent), +10% modifier -> 110.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::GrowthRate, 10.0, ModifierOp::AddPercent), "policy"),
    };
    CHECK(ResolveStatModifiers(effects, 100.0).total == Approx(110.0));
}

TEST_CASE("ResolveStatModifiers: contributions are sorted by sourceId for deterministic breakdowns",
          "[effects][math]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Energy, 1.0), "zeta"),
        Active(pool.StatMod(StatId::Energy, 2.0), "alpha"),
        Active(pool.StatMod(StatId::Energy, 3.0), "midway"),
    };

    const StatBreakdown_t breakdown = ResolveStatModifiers(effects);
    REQUIRE(breakdown.contributions.size() == 3);
    CHECK(breakdown.contributions[0].sourceId == "alpha");
    CHECK(breakdown.contributions[1].sourceId == "midway");
    CHECK(breakdown.contributions[2].sourceId == "zeta");
}

TEST_CASE("ResolveStatModifiers: breakdown records amount and op per contribution", "[effects][math]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Minerals, 25.0, ModifierOp::AddPercent), "bonus"),
    };

    const StatBreakdown_t breakdown = ResolveStatModifiers(effects);
    REQUIRE(breakdown.contributions.size() == 1);
    CHECK(breakdown.contributions[0].amount == Approx(25.0));
    CHECK(breakdown.contributions[0].op == ModifierOp::AddPercent);
    CHECK(breakdown.contributions[0].sourceId == "bonus");
}

TEST_CASE("ResolveStatModifiers: non-StatModifier effects and null configs are ignored", "[effects][math]")
{
    actest::EffectPool pool;
    std::vector<ActiveEffect_t> effects = {
        Active(pool.RuleFlag(RuleFlagId::Flight), "flag_source"),
        Active(pool.StatMod(StatId::Energy, 4.0), "real"),
    };
    effects.push_back(ActiveEffect_t{nullptr, "null_config", nullptr});

    const StatBreakdown_t breakdown = ResolveStatModifiers(effects);
    CHECK(breakdown.total == Approx(4.0));
    CHECK(breakdown.contributions.size() == 1);
}

TEST_CASE("ResolveStatModifiers: does NOT itself filter by stat — callers must pre-filter", "[effects][math]")
{
    // ResolveStatModifiers resolves every StatModifier handed to it, regardless of stat id.
    // The stat split is FilterByStatId's job. This pins the division of responsibility: passing
    // an unfiltered list double-counts across stats.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId::Nutrients, 2.0), "a"),
        Active(pool.StatMod(StatId::Minerals, 3.0), "b"),
    };
    CHECK(ResolveStatModifiers(effects).total == Approx(5.0));
}
