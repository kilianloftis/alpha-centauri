// Tests for ExpandGrantBuildingEffects: granted buildings are not stored on the base;
// their effects are discovered dynamically by expanding GrantBuildingEffect_t entries.

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "lib/effects/ActiveEffect.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

// Sum of Add-amounts for a stat, restricted to effects attributed to a given origin base
// (nullptr = globally applied effects).
double TotalFor(const std::vector<ActiveEffect_t>& effects, StatId stat, const BaseManager* pOrigin)
{
    std::vector<ActiveEffect_t> subset;
    for (const ActiveEffect_t& effect : FilterByStatId(effects, stat))
    {
        if (effect.originBase == pOrigin)
        {
            subset.push_back(effect);
        }
    }
    return ResolveStatModifiers(subset, 0.0).total;
}

int CountBySource(const std::vector<ActiveEffect_t>& effects, const std::string& sourceId)
{
    int count = 0;
    for (const ActiveEffect_t& effect : effects)
    {
        if (effect.sourceId == sourceId)
        {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("ExpandGrantBuildingEffects: a ThisBase-scoped grant expands the granted building for that base",
          "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    baseA.GetBuildingManager().AddBuilding("grantor_local");
    const auto expanded = ExpandGrantBuildingEffects(
        baseA.CollectBuildingEffects(), fixture.buildings(), {&baseA});

    // grantor_local's own +2 nutrients.
    CHECK(TotalFor(expanded, StatId::Nutrients, &baseA) == 2.0);
    // granted_hall's ThisBase +3 minerals, attributed to baseA with a chained sourceId.
    CHECK(TotalFor(expanded, StatId::Minerals, &baseA) == 3.0);
    CHECK(CountBySource(expanded, "grantor_local -> granted_hall") >= 1);
    // granted_hall's FactionGlobal +1 energy comes along with the grant.
    CHECK(TotalFor(expanded, StatId::Energy, nullptr) == 1.0);
}

TEST_CASE("ExpandGrantBuildingEffects: Instantaneous effects of the granted building are not expanded",
          "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    baseA.GetBuildingManager().AddBuilding("grantor_local");
    const auto expanded = ExpandGrantBuildingEffects(
        baseA.CollectBuildingEffects(), fixture.buildings(), {&baseA});

    // granted_hall declares an Instantaneous GrantTech; it must not appear in the pool.
    for (const ActiveEffect_t& effect : expanded)
    {
        REQUIRE(effect.config != nullptr);
        CHECK(effect.config->persistence == EffectPersistence_t::Continuous);
    }
}

TEST_CASE("ExpandGrantBuildingEffects: an unknown granted building id is skipped gracefully",
          "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    baseA.GetBuildingManager().AddBuilding("grantor_unknown");
    const auto collected = baseA.CollectBuildingEffects();
    const auto expanded = ExpandGrantBuildingEffects(collected, fixture.buildings(), {&baseA});

    // Nothing added beyond the collected effects (the grant itself is still in the list).
    CHECK(expanded.size() == collected.size());
}

TEST_CASE("ExpandGrantBuildingEffects: the same building granted twice in one base expands once",
          "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    // Both grantor_local and nested_middle grant granted_hall.
    baseA.GetBuildingManager().AddBuilding("grantor_local");
    baseA.GetBuildingManager().AddBuilding("nested_middle");
    const auto expanded = ExpandGrantBuildingEffects(
        baseA.CollectBuildingEffects(), fixture.buildings(), {&baseA});

    // granted_hall's +3 minerals must be counted once, not twice.
    CHECK(TotalFor(expanded, StatId::Minerals, &baseA) == 3.0);
}

TEST_CASE("ExpandGrantBuildingEffects: two bases granting the same building expand independently",
          "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);
    BaseManager& baseB = fixture.MakeBase(6, 6);

    baseA.GetBuildingManager().AddBuilding("grantor_local");
    baseB.GetBuildingManager().AddBuilding("grantor_local");

    std::vector<ActiveEffect_t> collected = baseA.CollectBuildingEffects();
    const auto fromB = baseB.CollectBuildingEffects();
    collected.insert(collected.end(), fromB.begin(), fromB.end());

    const auto expanded = ExpandGrantBuildingEffects(std::move(collected), fixture.buildings(),
                                                     {&baseA, &baseB});

    CHECK(TotalFor(expanded, StatId::Minerals, &baseA) == 3.0);
    CHECK(TotalFor(expanded, StatId::Minerals, &baseB) == 3.0);
}

TEST_CASE("ExpandGrantBuildingEffects: a faction-global grant clones ThisBase sub-effects once per base",
          "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);
    BaseManager& baseB = fixture.MakeBase(6, 6);

    // grantor_global grants granted_hall at FactionGlobal scope (no origin base).
    baseA.GetBuildingManager().AddBuilding("grantor_global");

    std::vector<ActiveEffect_t> collected = baseA.CollectBuildingEffects();
    const auto fromB = baseB.CollectBuildingEffects();
    collected.insert(collected.end(), fromB.begin(), fromB.end());

    const auto expanded = ExpandGrantBuildingEffects(std::move(collected), fixture.buildings(),
                                                     {&baseA, &baseB});

    // granted_hall's ThisBase +3 minerals lands on every base...
    CHECK(TotalFor(expanded, StatId::Minerals, &baseA) == 3.0);
    CHECK(TotalFor(expanded, StatId::Minerals, &baseB) == 3.0);
    // ...while its FactionGlobal +1 energy applies exactly once.
    CHECK(TotalFor(expanded, StatId::Energy, nullptr) == 1.0);
}

TEST_CASE("ExpandGrantBuildingEffects: nested grants expand recursively with a chained sourceId",
          "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    // nested_outer grants nested_middle (+4 energy), which grants granted_hall (+3 minerals).
    baseA.GetBuildingManager().AddBuilding("nested_outer");
    const auto expanded = ExpandGrantBuildingEffects(
        baseA.CollectBuildingEffects(), fixture.buildings(), {&baseA});

    CHECK(TotalFor(expanded, StatId::Energy, &baseA) == 4.0);
    CHECK(TotalFor(expanded, StatId::Minerals, &baseA) == 3.0);
    CHECK(CountBySource(expanded, "nested_outer -> nested_middle") >= 1);
    CHECK(CountBySource(expanded, "nested_outer -> nested_middle -> granted_hall") >= 1);
}

TEST_CASE("ExpandGrantBuildingEffects: mutually-granting buildings terminate", "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    // cycle_a grants cycle_b; cycle_b grants cycle_a. The expansion must not loop forever.
    baseA.GetBuildingManager().AddBuilding("cycle_a");
    const auto expanded = ExpandGrantBuildingEffects(
        baseA.CollectBuildingEffects(), fixture.buildings(), {&baseA});

    // cycle_b's +10 minerals is granted exactly once.
    int cycleBEffects = 0;
    for (const ActiveEffect_t& effect : FilterByStatId(expanded, StatId::Minerals))
    {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        if (pMod && pMod->amount == 10.0)
        {
            ++cycleBEffects;
        }
    }
    CHECK(cycleBEffects == 1);
}

TEST_CASE("ExpandGrantBuildingEffects: a cycle does not duplicate the originating building's effects",
          "[effects][grant]")
{
    // In a cycle a -> b -> a, the constructed cycle_a's own +1 minerals is collected once from
    // the base; the b -> a back-grant targets a building already in its own grant chain and
    // must be skipped, giving 11 minerals (1 + 10) rather than 12.
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    baseA.GetBuildingManager().AddBuilding("cycle_a");
    const auto expanded = ExpandGrantBuildingEffects(
        baseA.CollectBuildingEffects(), fixture.buildings(), {&baseA});

    CHECK(TotalFor(expanded, StatId::Minerals, &baseA) == 11.0);
}

TEST_CASE("Instantaneous GrantBuilding effects never enter the active pool, so they do not expand",
          "[effects][grant]")
{
    actest::BaseFixture fixture;
    BaseManager& baseA = fixture.MakeBase(2, 2);

    baseA.GetBuildingManager().AddBuilding("instant_grantor");
    const auto expanded = ExpandGrantBuildingEffects(
        baseA.CollectBuildingEffects(), fixture.buildings(), {&baseA});

    // instant_grantor's only effect is an Instantaneous GrantBuilding(flat_nutrient); it is
    // dispatched at construction time (DispatchInstantaneousEffects), not collected here.
    CHECK(expanded.empty());
    CHECK(TotalFor(expanded, StatId::Nutrients, &baseA) == 0.0);
}
