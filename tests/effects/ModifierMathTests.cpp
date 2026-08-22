// Tests for the modifier-combination math: ApplyModifierStack and ResolveStatModifiers.
//
// Documented contract (docs/architecture/effects-system.md):
//   total = (baseValue + sum of Add amounts)
//         * (1 + sum of AddPercent amounts / 100)   <- one arithmetic factor, percent points
//         * (product of MultiplyGeometric factors)

#include "TestHelpers.h"

#include "game/effects/ActiveEffect.h"
#include "game/map/Tile.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <ranges>

using namespace ac;
using actest::Active;
using Catch::Approx;

namespace
{
std::vector<std::pair<double, ModifierOp_t>> Stack(std::initializer_list<std::pair<double, ModifierOp_t>> init)
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
    CHECK(ApplyModifierStack(0.0, Stack({{2.0, ModifierOp_t::Add}})) == Approx(2.0));
    CHECK(ApplyModifierStack(1.0, Stack({{2.0, ModifierOp_t::Add}, {3.0, ModifierOp_t::Add}})) == Approx(6.0));
    CHECK(ApplyModifierStack(4.0, Stack({{-1.0, ModifierOp_t::Add}})) == Approx(3.0));
}

TEST_CASE("ApplyModifierStack: AddPercent is percent points on top of 100%", "[effects][math]")
{
    // 25 means +25%, matching the UI display.
    CHECK(ApplyModifierStack(4.0, Stack({{25.0, ModifierOp_t::AddPercent}})) == Approx(5.0));
    CHECK(ApplyModifierStack(4.0, Stack({{-25.0, ModifierOp_t::AddPercent}})) == Approx(3.0));
}

TEST_CASE("FinalizeResolvedStat uses lround for half values", "[effects][math][rounding]")
{
    CHECK(FinalizeResolvedStat(2.5) == 3);
    CHECK(FinalizeResolvedStat(1.5) == 2);
    CHECK(FinalizeResolvedStat(2.4) == 2);
    // Strength-2 attacker with +25% → 2.5 → 3 (shared ResolveStat / combat rule).
    CHECK(FinalizeResolvedStat(ApplyModifierStack(2.0, Stack({{25.0, ModifierOp_t::AddPercent}})))
          == 3);
}

TEST_CASE("ApplyModifierStack: multiple AddPercent contributions combine arithmetically, not geometrically",
          "[effects][math]")
{
    // +25% and +25% must be x1.5 (1 + 0.25 + 0.25), NOT x1.5625 (1.25 * 1.25).
    CHECK(ApplyModifierStack(4.0, Stack({{25.0, ModifierOp_t::AddPercent}, {25.0, ModifierOp_t::AddPercent}}))
          == Approx(6.0));
    // +50% and -50% cancel exactly.
    CHECK(ApplyModifierStack(8.0, Stack({{50.0, ModifierOp_t::AddPercent}, {-50.0, ModifierOp_t::AddPercent}}))
          == Approx(8.0));
}

TEST_CASE("ApplyModifierStack: -100% floors the arithmetic factor at zero output", "[effects][math]")
{
    CHECK(ApplyModifierStack(10.0, Stack({{-100.0, ModifierOp_t::AddPercent}})) == Approx(0.0));
}

TEST_CASE("ApplyModifierStack: MultiplyGeometric factors multiply the running total", "[effects][math]")
{
    // Factor form: 0.5 halves.
    CHECK(ApplyModifierStack(8.0, Stack({{0.5, ModifierOp_t::MultiplyGeometric}})) == Approx(4.0));
    CHECK(ApplyModifierStack(8.0, Stack({{0.5, ModifierOp_t::MultiplyGeometric}, {0.5, ModifierOp_t::MultiplyGeometric}}))
          == Approx(2.0));
    CHECK(ApplyModifierStack(3.0, Stack({{2.0, ModifierOp_t::MultiplyGeometric}})) == Approx(6.0));
}

TEST_CASE("ApplyModifierStack: combined ops follow (base+adds) * arithmetic * geometric", "[effects][math]")
{
    // (2 + 4) * (1 + 0.5) * 0.5 = 4.5
    const auto stack = Stack({{4.0, ModifierOp_t::Add},
                              {50.0, ModifierOp_t::AddPercent},
                              {0.5, ModifierOp_t::MultiplyGeometric}});
    CHECK(ApplyModifierStack(2.0, stack) == Approx(4.5));
}

TEST_CASE("ApplyModifierStack: clamps bound the value after the arithmetic", "[effects][math]")
{
    // The clamp sees (2 + 4) * 1.5 = 9, not the 2.0 seed.
    const auto capped = Stack({{4.0, ModifierOp_t::Add},
                               {50.0, ModifierOp_t::AddPercent},
                               {5.0, ModifierOp_t::MaxClamp}});
    CHECK(ApplyModifierStack(2.0, capped) == Approx(5.0));

    // A clamp that is not reached leaves the value alone.
    CHECK(ApplyModifierStack(2.0, Stack({{99.0, ModifierOp_t::MaxClamp}})) == Approx(2.0));
    CHECK(ApplyModifierStack(2.0, Stack({{1.0, ModifierOp_t::MinClamp}})) == Approx(2.0));
    CHECK(ApplyModifierStack(2.0, Stack({{7.0, ModifierOp_t::MinClamp}})) == Approx(7.0));
}

TEST_CASE("ApplyModifierStack: the tightest clamp of each kind wins", "[effects][math]")
{
    CHECK(ApplyModifierStack(10.0, Stack({{5.0, ModifierOp_t::MaxClamp},
                                          {3.0, ModifierOp_t::MaxClamp}})) == Approx(3.0));
    CHECK(ApplyModifierStack(0.0, Stack({{5.0, ModifierOp_t::MinClamp},
                                         {7.0, ModifierOp_t::MinClamp}})) == Approx(7.0));

    // Crossed bounds: MinClamp is applied last and therefore wins.
    CHECK(ApplyModifierStack(4.0, Stack({{1.0, ModifierOp_t::MaxClamp},
                                         {6.0, ModifierOp_t::MinClamp}})) == Approx(6.0));
}

TEST_CASE("ApplyModifierStack: a MaxClamp 0 zeroes a RawScaled seed", "[effects][math]")
{
    // How Perimeter Defense and Citizen difficulty cancel a pop-loss seed.
    CHECK(ApplyModifierStack(3.0, Stack({{0.0, ModifierOp_t::MaxClamp}})) == Approx(0.0));
}

TEST_CASE("ApplyModifierStack: result does not depend on contribution order", "[effects][math]")
{
    // An Add listed after a percent/geometric contribution still lands in the additive pool.
    const auto ordered  = Stack({{4.0, ModifierOp_t::Add},
                                 {50.0, ModifierOp_t::AddPercent},
                                 {0.5, ModifierOp_t::MultiplyGeometric}});
    const auto reversed = Stack({{0.5, ModifierOp_t::MultiplyGeometric},
                                 {50.0, ModifierOp_t::AddPercent},
                                 {4.0, ModifierOp_t::Add}});
    CHECK(ApplyModifierStack(2.0, ordered) == Approx(ApplyModifierStack(2.0, reversed)));
}

TEST_CASE("ResolveStatModifiers: empty input resolves to the base value", "[effects][math]")
{
    CHECK(ResolveStatModifiers(std::vector<ActiveEffect_t>{}, 0.0).total == Approx(0.0));
    CHECK(ResolveStatModifiers(std::vector<ActiveEffect_t>{}, 7.0).total == Approx(7.0));
    CHECK(ResolveStatModifiers(std::vector<ActiveEffect_t>{}, 0.0).contributions.empty());
}

TEST_CASE("ResolveStatModifiers: sums Add contributions from active effects", "[effects][math]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Nutrients, 2.0), "building_a"),
        Active(pool.StatMod(StatId_t::Nutrients, 3.0), "building_b"),
    };

    const StatBreakdown_t breakdown = ResolveStatModifiers(effects, 0.0);
    CHECK(breakdown.total == Approx(5.0));
    REQUIRE(breakdown.contributions.size() == 2);
}

TEST_CASE("ResolveStatModifiers: pure-multiplier stats require an explicit non-zero seed",
          "[effects][math]")
{
    // A stat that only ever receives AddPercent/MultiplyGeometric contributions collapses to
    // 0 from a 0 base — which is why baseValue has no default and every caller must state
    // its seed (as UnitDesign::GetBaseCost does with 1.0 for CostMultiplier).
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::CostMultiplier, 1.25, ModifierOp_t::MultiplyGeometric), "component"),
    };

    CHECK(ResolveStatModifiers(effects, 0.0).total == Approx(0.0));
    CHECK(ResolveStatModifiers(effects, 1.0).total == Approx(1.25));
}

TEST_CASE("ResolveStatModifiers: seeded base value participates in percent scaling", "[effects][math]")
{
    // GrowthRate-style usage: base 100 (percent), +10% modifier -> 110.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::GrowthRate, 10.0, ModifierOp_t::AddPercent), "policy"),
    };
    CHECK(ResolveStatModifiers(effects, 100.0).total == Approx(110.0));
}

TEST_CASE("ResolveStatModifiers: contributions are sorted by sourceId for deterministic breakdowns",
          "[effects][math]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Energy, 1.0), "zeta"),
        Active(pool.StatMod(StatId_t::Energy, 2.0), "alpha"),
        Active(pool.StatMod(StatId_t::Energy, 3.0), "midway"),
    };

    const StatBreakdown_t breakdown = ResolveStatModifiers(effects, 0.0);
    REQUIRE(breakdown.contributions.size() == 3);
    CHECK(breakdown.contributions[0].sourceId == "alpha");
    CHECK(breakdown.contributions[1].sourceId == "midway");
    CHECK(breakdown.contributions[2].sourceId == "zeta");
}

TEST_CASE("ResolveStatModifiers: breakdown records amount and op per contribution", "[effects][math]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Minerals, 25.0, ModifierOp_t::AddPercent), "bonus"),
    };

    const StatBreakdown_t breakdown = ResolveStatModifiers(effects, 0.0);
    REQUIRE(breakdown.contributions.size() == 1);
    CHECK(breakdown.contributions[0].amount == Approx(25.0));
    CHECK(breakdown.contributions[0].op == ModifierOp_t::AddPercent);
    CHECK(breakdown.contributions[0].sourceId == "bonus");
}

TEST_CASE("ResolveStatModifiers: non-StatModifier effects are ignored", "[effects][math]")
{
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.RuleFlag(RuleFlagId_t::ForcesPsiCombat), "flag_source"),
        Active(pool.StatMod(StatId_t::Energy, 4.0), "real"),
    };

    const StatBreakdown_t breakdown = ResolveStatModifiers(effects, 0.0);
    CHECK(breakdown.total == Approx(4.0));
    CHECK(breakdown.contributions.size() == 1);
    CHECK(effects[1].config != nullptr); // ActiveEffect_t always carries a config
}

TEST_CASE("ResolveStatModifiers: does NOT itself filter by stat — callers must pre-filter", "[effects][math]")
{
    // ResolveStatModifiers resolves every StatModifier handed to it, regardless of stat id.
    // The stat split is FilterByStatId's job. This pins the division of responsibility: passing
    // an unfiltered list double-counts across stats.
    actest::EffectPool pool;
    const std::vector<ActiveEffect_t> effects = {
        Active(pool.StatMod(StatId_t::Nutrients, 2.0), "a"),
        Active(pool.StatMod(StatId_t::Minerals, 3.0), "b"),
    };
    CHECK(ResolveStatModifiers(effects, 0.0).total == Approx(5.0));
}

TEST_CASE("ResolveStatModifiers: amount_source ElevationEnergySeed scales seed by amount",
          "[effects][math][amount_source]")
{
    actest::EffectPool pool;
    StatModifierEffect_t mod;
    mod.stat = StatId_t::Energy;
    mod.amount = 2.0;
    mod.op = ModifierOp_t::Add;
    mod.amountSource = StatModifierEffect_t::AmountSource_t::ElevationEnergySeed;

    EffectConfig_t config;
    config.effect = mod;
    config.scope = EffectScope_t::ThisTile;
    config.persistence = EffectPersistence_t::Continuous;
    const EffectConfig_t& rConfig = pool.Add(std::move(config));

    Tile tile(0, 0);
    tile.SetElevation(2000); // seed = 2
    const EffectContext_t ctx{&tile};
    const std::vector<ActiveEffect_t> effects = {Active(rConfig, "solar")};

    CHECK(ResolveStatModifiers(effects, 0.0, &ctx).total == Approx(4.0)); // 2 * 2
    CHECK(ResolveStatModifiers(effects, 0.0, nullptr).total == Approx(0.0));
    CHECK(std::ranges::distance(FilterByStatId(effects, StatId_t::Energy)) == 0);
    CHECK(std::ranges::distance(FilterByStatIdInContext(effects, StatId_t::Energy, ctx)) == 1);
}

TEST_CASE("ResolveStatModifiers: amount_source MineralsConverted scales by minerals",
          "[effects][math][amount_source]")
{
    actest::EffectPool pool;
    StatModifierEffect_t mod;
    mod.stat = StatId_t::Energy;
    mod.amount = 0.5;
    mod.op = ModifierOp_t::Add;
    mod.amountSource = StatModifierEffect_t::AmountSource_t::MineralsConverted;

    EffectConfig_t config;
    config.effect = mod;
    config.scope = EffectScope_t::ThisBase;
    config.persistence = EffectPersistence_t::Continuous;
    const EffectConfig_t& rConfig = pool.Add(std::move(config));

    const EffectContext_t ctx{.mineralsConverted = 5};
    const std::vector<ActiveEffect_t> effects = {Active(rConfig, "stockpile")};

    CHECK(ResolveStatModifiers(effects, 0.0, &ctx).total == Approx(2.5));
    CHECK(ResolveStatModifiers(effects, 0.0, nullptr).total == Approx(0.0));
    CHECK(std::ranges::distance(FilterByStatId(effects, StatId_t::Energy)) == 0);
}
