// Tests for post-load effect reference validation (EffectReferenceValidator) and the
// compile-time single sources of truth: scope routing (LaneFor/IsFactionLane) and stat
// seed semantics (KindFor/SeedFor).

#include "game/EffectReferenceValidator.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/faction/FactionRegistry.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/TerrainFeatureValidation.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/research/TechRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/ProbeActionConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/effects/EffectConfig.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

using namespace ac;

namespace
{

EffectConfig_t GrantBuilding(std::string buildingId)
{
    EffectConfig_t config;
    config.effect = GrantBuildingEffect_t{std::move(buildingId)};
    config.scope = EffectScope_t::ThisBase;
    config.persistence = EffectPersistence_t::Continuous;
    return config;
}

// Empty instances of every target / effect-source LoadGameData installs before validation.
void FillEffectReferenceContext(GameDataContext& rData)
{
    rData.buildingRegistry = std::make_unique<BuildingRegistry>();
    rData.improvementRegistry = std::make_unique<ImprovementRegistry>();
    rData.techRegistry = std::make_unique<TechRegistry>();
    rData.unitComponentRegistry = std::make_unique<UnitComponentRegistry>();
    rData.popTypeRegistry = std::make_unique<PopTypeRegistry>();
    rData.socialPolicyRegistry = std::make_unique<SocialPolicyRegistry>();
    rData.socialRatingRegistry = std::make_unique<SocialRatingRegistry>();
    rData.factionRegistry = std::make_unique<FactionRegistry>();
    rData.councilProposalRegistry = std::make_unique<CouncilProposalRegistry>();
    rData.councilRules = std::make_unique<CouncilRulesConfig_t>();
    rData.probeActionsConfig = std::make_unique<ProbeActionsConfig_t>();
}

} // namespace

// LaneFor is the compile-time single source of truth for scope routing; pin every scope's
// lane so a new scope (which the compiler forces into LaneFor's switch) gets a deliberate
// routing decision here too.
static_assert(LaneFor(EffectScope_t::ThisBase) == EffectLane_t::Base);
static_assert(LaneFor(EffectScope_t::AllOwnerBases) == EffectLane_t::FactionWide);
static_assert(LaneFor(EffectScope_t::FactionGlobal) == EffectLane_t::FactionWide);
static_assert(LaneFor(EffectScope_t::WorldGlobal) == EffectLane_t::FactionWide);
static_assert(LaneFor(EffectScope_t::FactionUnits) == EffectLane_t::FactionUnits);
static_assert(LaneFor(EffectScope_t::ProducedAtThisBase) == EffectLane_t::ProducedAtBase);
static_assert(LaneFor(EffectScope_t::ThisUnit) == EffectLane_t::UnitLocal);
static_assert(LaneFor(EffectScope_t::ThisPop) == EffectLane_t::PopLocal);
static_assert(LaneFor(EffectScope_t::ThisTile) == EffectLane_t::TileLocal);

static_assert(!IsFactionLane(EffectScope_t::ThisBase));
static_assert(IsFactionLane(EffectScope_t::AllOwnerBases));
static_assert(IsFactionLane(EffectScope_t::FactionGlobal));
static_assert(IsFactionLane(EffectScope_t::WorldGlobal));
static_assert(IsFactionLane(EffectScope_t::FactionUnits));
static_assert(!IsFactionLane(EffectScope_t::ProducedAtThisBase));
static_assert(!IsFactionLane(EffectScope_t::ThisUnit));
static_assert(!IsFactionLane(EffectScope_t::ThisPop));
static_assert(!IsFactionLane(EffectScope_t::ThisTile));

static_assert(TagsOriginBase(EffectScope_t::ThisBase));
static_assert(TagsOriginBase(EffectScope_t::ProducedAtThisBase));
static_assert(TagsOriginBase(EffectScope_t::FactionUnits));
static_assert(!TagsOriginBase(EffectScope_t::AllOwnerBases));
static_assert(!TagsOriginBase(EffectScope_t::FactionGlobal));
static_assert(!TagsOriginBase(EffectScope_t::WorldGlobal));
static_assert(!TagsOriginBase(EffectScope_t::ThisUnit));
static_assert(!TagsOriginBase(EffectScope_t::ThisPop));
static_assert(!TagsOriginBase(EffectScope_t::ThisTile));

// KindFor is the same single-source-of-truth pattern for stat seed semantics: pin every
// stat's kind so a new StatId_t (which the compiler forces into KindFor's switch) gets a
// deliberate seed decision here too.
static_assert(KindFor(StatId_t::Nutrients) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Minerals) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Energy) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Econ) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Labs) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Psych) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Drones) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Talents) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Attack) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Defense) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Movement) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::Vision) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::HitPoints) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::PsiDamage) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::DisengageChance) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::TurnsOfFuel) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::DamageFromOutOfFuel) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CargoCapacity) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::DifficultTerrainCost) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::StartingExperience) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::MoraleBonus) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CostMultiplier) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::ProbeActionCost) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::ProbeDefense) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::ProbeFailureScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::ProbeSuccessScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::PositiveMoraleScale) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::GrowthRate) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::TechCost) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::MoistureTier) == StatKind_t::RawScaled);
static_assert(KindFor(StatId_t::CommerceRate) == StatKind_t::PureMultiplier);
static_assert(KindFor(StatId_t::CouncilVotes) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::CommerceEnergyBonus) == StatKind_t::Additive);
static_assert(KindFor(StatId_t::InefficiencyDenominator) == StatKind_t::Additive);

// SeedFor derives the context-free seed from the kind; RawScaled stats have none (SeedFor
// throws for them, which is not constexpr-evaluable, so no pin here).
static_assert(SeedFor(StatId_t::Nutrients) == 0.0);
static_assert(SeedFor(StatId_t::Attack) == 0.0);
static_assert(SeedFor(StatId_t::CostMultiplier) == 1.0);

TEST_CASE("ValidateEffectReferences: GrantBuilding targets must exist", "[effects][validation]")
{
    BuildingRegistry buildings;
    buildings.Load(actest::FixturePath("buildings.json"));

    const std::vector<EffectConfig_t> good = {GrantBuilding("granted_hall")};
    CHECK_NOTHROW(ValidateEffectReferences(good, "src", &buildings, nullptr, nullptr));

    const std::vector<EffectConfig_t> bad = {GrantBuilding("no_such_building")};
    CHECK_THROWS_WITH(ValidateEffectReferences(bad, "src", &buildings, nullptr, nullptr),
                      Catch::Matchers::ContainsSubstring("no_such_building"));

    // A null registry skips the check (partial validation context).
    CHECK_NOTHROW(ValidateEffectReferences(bad, "src", nullptr, nullptr, nullptr));
}

TEST_CASE("ValidateEffectReferences: social rating axes must have a table",
          "[effects][validation]")
{
    // SocialRatingResolver looks up the axis table whenever the accumulated total is non-zero,
    // which is the first turn after a player adopts a policy declaring the modifier. A modifier
    // naming an axis with no table has to fail at load, not there.
    SocialRatingRegistry ratings;
    ratings.Load(actest::FixturePath("social_rating_effects.json"));

    const auto ratingModifier = [](SocialRatingId_t rating) {
        EffectConfig_t config;
        config.effect = SocialRatingModifierEffect_t{rating, 2};
        config.scope = EffectScope_t::FactionGlobal;
        config.persistence = EffectPersistence_t::Continuous;
        return std::vector<EffectConfig_t>{config};
    };

    CHECK_NOTHROW(ValidateEffectReferences(ratingModifier(SocialRatingId_t::Growth), "policy_x",
                                           nullptr, nullptr, nullptr, nullptr, &ratings));

    // The fixture defines no table for Police.
    REQUIRE(ratings.Find("police") == nullptr);
    CHECK_THROWS_WITH(ValidateEffectReferences(ratingModifier(SocialRatingId_t::Police),
                                               "policy_x", nullptr, nullptr, nullptr, nullptr,
                                               &ratings),
                      Catch::Matchers::ContainsSubstring("police")
                          && Catch::Matchers::ContainsSubstring("policy_x"));

    // A null registry skips the check (partial validation context).
    CHECK_NOTHROW(ValidateEffectReferences(ratingModifier(SocialRatingId_t::Police), "policy_x",
                                           nullptr, nullptr, nullptr, nullptr, nullptr));
}

TEST_CASE("ValidateEffectReferences: selector improvement ids must exist", "[effects][validation]")
{
    ImprovementRegistry improvements;
    improvements.Load(actest::FixturePath("improvements.json"));

    actest::EffectPool pool;
    const std::vector<EffectConfig_t> good = {
        pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
                     actest::ImprovementSelector("Farm"))};
    CHECK_NOTHROW(ValidateEffectReferences(good, "src", nullptr, &improvements, nullptr));

    const std::vector<EffectConfig_t> bad = {
        pool.StatMod(StatId_t::Nutrients, 1.0, ModifierOp_t::Add, EffectScope_t::ThisBase,
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
    // "Rocky" / "Base" / "Water" are all improvement ids — condition features have no
    // non-registry special cases.
    const std::vector<EffectConfig_t> good = {
        pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Rocky")),
        pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Base")),
        pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Water"))};
    CHECK_NOTHROW(ValidateEffectReferences(good, "src", nullptr, &improvements, nullptr));

    const std::vector<EffectConfig_t> bad = {
        pool.StatMod(StatId_t::Attack, 25.0, ModifierOp_t::AddPercent, EffectScope_t::ThisUnit,
                     std::nullopt, actest::TargetTileHas("Swamp"))};
    CHECK_THROWS_WITH(ValidateEffectReferences(bad, "src", nullptr, &improvements, nullptr),
                      Catch::Matchers::ContainsSubstring("Swamp"));
}

TEST_CASE("ValidateTerrainFeatures: every intrinsic terrain id must have an improvement entry",
          "[effects][validation][terrain]")
{
    ImprovementRegistry complete;
    complete.Load(actest::FixturePath("improvements.json"));
    CHECK_NOTHROW(ValidateTerrainFeatures(complete));

    // Same file minus "Aquifer" — without this check Tile would silently drop the feature.
    ImprovementRegistry incomplete;
    incomplete.Load(actest::FixturePath("improvements_missing_terrain.json"));
    CHECK_THROWS_WITH(ValidateTerrainFeatures(incomplete),
                      Catch::Matchers::ContainsSubstring("Aquifer"));
}

TEST_CASE("ValidateEffectReferences: HasComponent unitFilter ids must exist",
          "[effects][validation][unitFilter]")
{
    UnitComponentRegistry components;
    components.Load(actest::FixturePath("unit_components.json"));

    actest::EffectPool pool;
    const std::vector<EffectConfig_t> good = {
        pool.StatMod(StatId_t::Attack, 1.0, ModifierOp_t::Add, EffectScope_t::FactionUnits,
                     std::nullopt, std::nullopt, EffectPersistence_t::Continuous,
                     actest::HasComponentFilter("test_weapon"))};
    CHECK_NOTHROW(ValidateEffectReferences(good, "src", nullptr, nullptr, nullptr, &components));

    const std::vector<EffectConfig_t> bad = {
        pool.StatMod(StatId_t::Attack, 1.0, ModifierOp_t::Add, EffectScope_t::FactionUnits,
                     std::nullopt, std::nullopt, EffectPersistence_t::Continuous,
                     actest::HasComponentFilter("no_such_component"))};
    CHECK_THROWS_WITH(
        ValidateEffectReferences(bad, "src", nullptr, nullptr, nullptr, &components),
        Catch::Matchers::ContainsSubstring("no_such_component"));

    // A null registry skips the check (partial validation context).
    CHECK_NOTHROW(ValidateEffectReferences(bad, "src", nullptr, nullptr, nullptr, nullptr));
}

TEST_CASE("ValidateEffectReferences(GameDataContext): null required registry throws",
          "[effects][validation]")
{
    GameDataContext empty;
    CHECK_THROWS_WITH(ValidateEffectReferences(empty),
                      Catch::Matchers::ContainsSubstring("buildingRegistry"));

    GameDataContext missingTech;
    FillEffectReferenceContext(missingTech);
    missingTech.techRegistry.reset();
    CHECK_THROWS_WITH(ValidateEffectReferences(missingTech),
                      Catch::Matchers::ContainsSubstring("techRegistry"));

    GameDataContext complete;
    FillEffectReferenceContext(complete);
    CHECK_NOTHROW(ValidateEffectReferences(complete));
}

TEST_CASE("ValidateEffectReferences(GameDataContext): probe action effect ids are checked",
          "[effects][validation][probe]")
{
    // Minimal building registry — the full buildings fixture grants techs that would
    // fail against an empty TechRegistry before the probe walk is reached.
    const std::filesystem::path buildingsPath =
        std::filesystem::temp_directory_path() / "ac_probe_effect_buildings.json";
    {
        std::ofstream out(buildingsPath);
        out << R"([
            { "id": "granted_hall", "name": "Granted Hall", "mineral_cost": 10, "category": "grow" }
        ])";
    }

    GameDataContext data;
    FillEffectReferenceContext(data);
    data.buildingRegistry->Load(buildingsPath.string());

    SECTION("known GrantBuilding on a probe action does not throw")
    {
        ProbeActionConfig_t action;
        action.id = ProbeActionId_t::Infiltrate;
        action.effects = {GrantBuilding("granted_hall")};
        data.probeActionsConfig->actions = {action};

        CHECK_NOTHROW(ValidateEffectReferences(data));
    }

    SECTION("unknown GrantBuilding throws naming the probe action")
    {
        ProbeActionConfig_t action;
        action.id = ProbeActionId_t::Infiltrate;
        action.effects = {GrantBuilding("no_such_building")};
        data.probeActionsConfig->actions = {action};

        CHECK_THROWS_WITH(ValidateEffectReferences(data),
                          Catch::Matchers::ContainsSubstring("probe_action:infiltrate")
                              && Catch::Matchers::ContainsSubstring("no_such_building"));
    }

    std::filesystem::remove(buildingsPath);
}
