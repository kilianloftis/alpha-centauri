// Tests for pop-type effect resolution: the ThisPop (tile multiplier) vs ThisBase (flat
// generation) split documented in "Pop Type Effects".

#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/faction/base/BaseTypes.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "lib/effects/ActiveEffect.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

TEST_CASE("Pop::ApplyTileMultipliers: a pop type without ThisPop effects leaves tile yield unchanged",
          "[effects][pop]")
{
    actest::PopTypeRegistryOnly reg;
    Pop worker(*reg.popTypes.Find("Worker"));

    const TileResources_t raw{4, 2, 3}; // nutrients, energy, minerals
    const TileResources_t result = worker.ApplyTileMultipliers(raw);
    CHECK(result.nutrients == 4);
    CHECK(result.energy == 2);
    CHECK(result.minerals == 3);
}

TEST_CASE("Pop::ApplyTileMultipliers: ThisPop AddPercent scales only the targeted resource",
          "[effects][pop]")
{
    // FungalFarmer: ThisPop +50% nutrients, plus a ThisBase +1 econ that must NOT leak in here.
    actest::PopTypeRegistryOnly reg;
    Pop farmer(*reg.popTypes.Find("FungalFarmer"));

    const TileResources_t raw{4, 2, 2};
    const TileResources_t result = farmer.ApplyTileMultipliers(raw);
    CHECK(result.nutrients == 6); // 4 * 1.5
    CHECK(result.energy == 2);
    CHECK(result.minerals == 2);
}

TEST_CASE("Pop::ApplyTileMultipliers: zero raw yield stays zero under percent multipliers",
          "[effects][pop]")
{
    actest::PopTypeRegistryOnly reg;
    Pop farmer(*reg.popTypes.Find("FungalFarmer"));

    const TileResources_t raw{0, 0, 0};
    const TileResources_t result = farmer.ApplyTileMultipliers(raw);
    CHECK(result.nutrients == 0);
    CHECK(result.energy == 0);
    CHECK(result.minerals == 0);
}

TEST_CASE("Pop::GetSpecialistOutput: resolves the ThisBase flat-generation subset", "[effects][pop]")
{
    actest::PopTypeRegistryOnly reg;

    const Pop doctor(*reg.popTypes.Find("Doctor"));
    const SpecialistOutput_t doctorOut = doctor.GetSpecialistOutput();
    CHECK(doctorOut.psych == 2);
    CHECK(doctorOut.econ == 0);
    CHECK(doctorOut.labs == 0);

    const Pop technician(*reg.popTypes.Find("Technician"));
    const SpecialistOutput_t techOut = technician.GetSpecialistOutput();
    CHECK(techOut.econ == 3);
    CHECK(techOut.psych == 0);
    CHECK(techOut.labs == 0);
}

TEST_CASE("ThisPop effects must never be resolved together with flat Add effects (documented invariant)",
          "[effects][pop]")
{
    // The doc calls this out explicitly: ResolveStatModifiers sums Add contributions into the
    // seeded base BEFORE the multiplicative step, so resolving a raw-seeded tile multiplier
    // together with a flat Add bonus would scale the flat bonus too. This test demonstrates
    // why the scope split exists: the combined resolve gives a DIFFERENT (wrong) result than
    // the correct split resolution.
    actest::EffectPool pool;
    const ActiveEffect_t multiplier =
        actest::Active(pool.StatMod(StatId::Nutrients, 50.0, ModifierOp::AddPercent,
                                    EffectScope_t::ThisPop), "tile_mult");
    const ActiveEffect_t flatBonus =
        actest::Active(pool.StatMod(StatId::Nutrients, 2.0, ModifierOp::Add,
                                    EffectScope_t::ThisBase), "flat");

    const double rawTileYield = 4.0;

    // Correct: multiplier scales the raw tile yield, flat bonus added separately.
    const double correct = ResolveStatModifiers({multiplier}, rawTileYield).total
                         + ResolveStatModifiers({flatBonus}, 0.0).total; // 6 + 2 = 8

    // Wrong: one combined resolve scales the flat bonus too: (4 + 2) * 1.5 = 9.
    const double combined = ResolveStatModifiers({multiplier, flatBonus}, rawTileYield).total;

    CHECK(correct == 8.0);
    CHECK(combined == 9.0);
    CHECK(correct != combined);
}
