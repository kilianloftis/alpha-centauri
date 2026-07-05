// Tests for post-load effect reference validation (EffectReferenceValidator) and the
// parse-time scope routing single source of truth (LaneFor/IsFactionLane).

#include "game/EffectReferenceValidator.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/ImprovementRegistry.h"
#include "lib/effects/BonusEffect.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace ac;

namespace
{

EffectConfig_t GrantBuilding_(std::string buildingId)
{
    EffectConfig_t config;
    config.effect = GrantBuildingEffect_t{std::move(buildingId)};
    config.scope = EffectScope_t::ThisBase;
    config.persistence = EffectPersistence_t::Continuous;
    return config;
}

} // namespace

// LaneFor is the compile-time single source of truth for scope routing; pin every scope's
// lane so a new scope (which the compiler forces into LaneFor's switch) gets a deliberate
// routing decision here too.
static_assert(LaneFor(EffectScope_t::ThisBase) == EffectLane::Base);
static_assert(LaneFor(EffectScope_t::AllOwnerBases) == EffectLane::FactionWide);
static_assert(LaneFor(EffectScope_t::FactionGlobal) == EffectLane::FactionWide);
static_assert(LaneFor(EffectScope_t::WorldGlobal) == EffectLane::FactionWide);
static_assert(LaneFor(EffectScope_t::FactionUnits) == EffectLane::FactionUnits);
static_assert(LaneFor(EffectScope_t::ThisUnit) == EffectLane::UnitLocal);
static_assert(LaneFor(EffectScope_t::ThisPop) == EffectLane::PopLocal);
static_assert(LaneFor(EffectScope_t::ThisTile) == EffectLane::TileLocal);

static_assert(!IsFactionLane(EffectScope_t::ThisBase));
static_assert(IsFactionLane(EffectScope_t::AllOwnerBases));
static_assert(IsFactionLane(EffectScope_t::FactionGlobal));
static_assert(IsFactionLane(EffectScope_t::WorldGlobal));
static_assert(IsFactionLane(EffectScope_t::FactionUnits));
static_assert(!IsFactionLane(EffectScope_t::ThisUnit));
static_assert(!IsFactionLane(EffectScope_t::ThisPop));
static_assert(!IsFactionLane(EffectScope_t::ThisTile));

TEST_CASE("ValidateEffectReferences: GrantBuilding targets must exist", "[effects][validation]")
{
    BuildingRegistry buildings;
    buildings.Load(actest::FixturePath("buildings.json"));

    const std::vector<EffectConfig_t> good = {GrantBuilding_("granted_hall")};
    CHECK_NOTHROW(ValidateEffectReferences(good, "src", &buildings, nullptr, nullptr));

    const std::vector<EffectConfig_t> bad = {GrantBuilding_("no_such_building")};
    CHECK_THROWS_WITH(ValidateEffectReferences(bad, "src", &buildings, nullptr, nullptr),
                      Catch::Matchers::ContainsSubstring("no_such_building"));

    // A null registry skips the check (partial validation context).
    CHECK_NOTHROW(ValidateEffectReferences(bad, "src", nullptr, nullptr, nullptr));
}

TEST_CASE("ValidateEffectReferences: selector improvement ids must exist", "[effects][validation]")
{
    ImprovementRegistry improvements;
    improvements.Load(actest::FixturePath("improvements.json"));

    actest::EffectPool pool;
    const std::vector<EffectConfig_t> good = {
        pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase,
                     actest::ImprovementSelector("Farm"))};
    CHECK_NOTHROW(ValidateEffectReferences(good, "src", nullptr, &improvements, nullptr));

    const std::vector<EffectConfig_t> bad = {
        pool.StatMod(StatId::Nutrients, 1.0, ModifierOp::Add, EffectScope_t::ThisBase,
                     actest::ImprovementSelector("Fram"))};
    CHECK_THROWS_WITH(ValidateEffectReferences(bad, "src", nullptr, &improvements, nullptr),
                      Catch::Matchers::ContainsSubstring("Fram"));
}

TEST_CASE("ValidateEffectReferences: condition features accept terrain ids and improvement ids",
          "[effects][validation]")
{
    ImprovementRegistry improvements;
    improvements.Load(actest::FixturePath("improvements.json"));

    actest::EffectPool pool;
    // "Rocky" is a terrain feature id; "Base" is an improvement id — both valid.
    const std::vector<EffectConfig_t> good = {
        pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Rocky")),
        pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Base"))};
    CHECK_NOTHROW(ValidateEffectReferences(good, "src", nullptr, &improvements, nullptr));

    const std::vector<EffectConfig_t> bad = {
        pool.StatMod(StatId::Attack, 25.0, ModifierOp::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Swamp"))};
    CHECK_THROWS_WITH(ValidateEffectReferences(bad, "src", nullptr, &improvements, nullptr),
                      Catch::Matchers::ContainsSubstring("Swamp"));
}
