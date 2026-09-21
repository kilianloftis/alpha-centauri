// Tests for the triggered-effect dispatcher: the results it reports back, the oncePer
// bookkeeping, and GrantUnit's anchor / placement rules.

#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <variant>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

// A live session with one faction, which the dispatcher needs for anything that touches
// GameState (unit ids, the world map, the diplomacy ledger).
struct TriggerGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pFaction = nullptr;

    TriggerGame_()
    {
        // GrantUnit assembles its design from the component registry on the data context,
        // which FactionFixture leaves unset (it keeps its own registry by value).
        fixtures.dataContext.unitComponentRegistry = std::make_unique<UnitComponentRegistry>();
        fixtures.dataContext.unitComponentRegistry->Load(FixturePath("unit_components.json"));

        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
            fixtures.dataContext.interactionGrids, actest::k_TestRngSeed);
        pFaction = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, actest::k_TestFactionSeed));
    }

    BaseManager& MakeBase(int x, int y)
    {
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = pFaction->CreateBase(
            pState->AllocateBaseId(), "TestBase", pTile, fixtures.dataContext,
            pState->GetTileEffects(), pState->GetSecretProjectAvailability());
        REQUIRE(pBase);
        return *pBase;
    }
};

TriggeredEffectConfig_t Effect_(TriggeredEffectVariant_t effect)
{
    TriggeredEffectConfig_t config;
    config.effect = std::move(effect);
    return config;
}

TriggeredEffectConfig_t OnceEffect_(TriggeredEffectVariant_t effect, OnceScope_t scope,
                                    std::string key)
{
    TriggeredEffectConfig_t config = Effect_(std::move(effect));
    config.oncePer = OncePer_t{scope, std::move(key)};
    return config;
}

const GrantUnitEffect_t k_ScoutGrant{{"test_chassis", "test_weapon"}, 1};

std::vector<TriggeredEffectConfig_t> GrantList_(GrantUnitEffect_t grant = k_ScoutGrant)
{
    return {Effect_(std::move(grant))};
}

int UnitCount_(const Faction& rFaction)
{
    return static_cast<int>(std::ranges::distance(rFaction.GetUnitManager().Units()));
}

const Unit& FirstUnit_(const Faction& rFaction)
{
    auto units = rFaction.GetUnitManager().Units();
    REQUIRE(std::ranges::distance(units) > 0);
    return *units.begin();
}

const Unit& LastUnit_(const Faction& rFaction)
{
    auto units = rFaction.GetUnitManager().Units();
    const auto count = std::ranges::distance(units);
    REQUIRE(count > 0);
    return *std::ranges::next(units.begin(), count - 1);
}

} // namespace

TEST_CASE("ApplyTriggeredEffects reports what each entry did", "[effects][triggered]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    base.GetBuildingManager().AddBuilding("flat_nutrient");

    const std::vector<TriggeredEffectConfig_t> effects = {
        Effect_(GrantTechEffect_t{"some_tech"}),
        Effect_(ModifyPopulationEffect_t{2, ModifierOp_t::Add, 0}),
        Effect_(DestroyFacilityEffect_t{1, true, false}),
    };

    TriggeredEffectContext_t context(*game.pState, base);
    const std::vector<TriggeredEffectResult_t> results = ApplyTriggeredEffects(effects, context);

    REQUIRE(results.size() == 3);
    CHECK(std::get<TechGranted_t>(results[0]).techId == "some_tech");
    CHECK(std::get<PopulationChanged_t>(results[1]).delta == 2);
    CHECK(std::get<FacilitiesDestroyed_t>(results[2]).buildingIds
          == std::vector<BuildingId_t>{"flat_nutrient"});

    CHECK(game.pFaction->GetResearch().HasDiscoveredTech("some_tech"));
    CHECK(base.GetBuildingManager().FindBuilding("flat_nutrient") == nullptr);
}

// The context carries what the trigger had. An entry needing a subject the trigger lacks is
// skipped rather than guessed at — a council vote has no base for a DestroyFacility to hit.
TEST_CASE("ApplyTriggeredEffects skips entries whose subject the context lacks",
          "[effects][triggered]")
{
    TriggerGame_ game;
    game.MakeBase(4, 4);

    const std::vector<TriggeredEffectConfig_t> effects = {
        Effect_(DestroyFacilityEffect_t{1, true, false}),
        Effect_(ModifyPopulationEffect_t{-1, ModifierOp_t::Add, 0}),
        Effect_(GrantTechEffect_t{"some_tech"}),
    };

    TriggeredEffectContext_t context(*game.pState, *game.pFaction); // no pBase
    const std::vector<TriggeredEffectResult_t> results = ApplyTriggeredEffects(effects, context);

    REQUIRE(results.size() == 1);
    CHECK(std::get<TechGranted_t>(results[0]).techId == "some_tech");
}

// A council vote lists every member, and GrantEnergy credits each. A base-subject effect in
// the same list must still fire once — it has one subject however long the faction list is.
TEST_CASE("A multi-faction context repeats faction-subject effects only",
          "[effects][triggered]")
{
    TriggerGame_ game;
    Faction& second = game.pState->AddFaction(std::make_unique<Faction>(
        game.pState->AllocateFactionId(), false, game.fixtures.factionDefinition,
        game.fixtures.dataContext, game.pState->GetWorldMap(), game.settings,
        actest::k_TestFactionSeed));
    BaseManager& base = game.MakeBase(4, 4);

    const std::vector<TriggeredEffectConfig_t> effects = {
        Effect_(GrantEnergyEffect_t{100}),
        Effect_(ModifyPopulationEffect_t{1, ModifierOp_t::Add, 0}),
    };
    const int sizeBefore = base.GetPopulation().GetSize();

    TriggeredEffectContext_t context(*game.pState, {game.pFaction, &second});
    context.pBase = &base;
    const std::vector<TriggeredEffectResult_t> results = ApplyTriggeredEffects(effects, context);

    // Two energy grants (one per member), one population change (one base).
    REQUIRE(results.size() == 3);
    CHECK(std::get<EnergyGranted_t>(results[0]).amount == 100);
    CHECK(std::get<EnergyGranted_t>(results[1]).amount == 100);
    CHECK(std::get<PopulationChanged_t>(results[2]).delta == 1);
    CHECK(base.GetPopulation().GetSize() == sizeBefore + 1);
}

TEST_CASE("oncePer fires an entry once per subject and lets its siblings repeat",
          "[effects][triggered][once]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);

    // The pattern the monolith needs: a one-shot grant beside a repeatable effect.
    const std::vector<TriggeredEffectConfig_t> effects = {
        OnceEffect_(GrantTechEffect_t{"some_tech"}, OnceScope_t::Base, "monolith_gift"),
        Effect_(ModifyPopulationEffect_t{1, ModifierOp_t::Add, 0}),
    };

    TriggeredEffectContext_t context(*game.pState, base);
    CHECK(ApplyTriggeredEffects(effects, context).size() == 2);

    // Second visit: the grant is spent, the repeatable effect still applies.
    const std::vector<TriggeredEffectResult_t> second = ApplyTriggeredEffects(effects, context);
    REQUIRE(second.size() == 1);
    CHECK(std::get<PopulationChanged_t>(second[0]).delta == 1);
}

// The key is authored, not derived from the entry, so two entries sharing one key consume
// each other — that is what makes "a second, different Monolith grants nothing" work.
TEST_CASE("oncePer entries sharing a key consume each other", "[effects][triggered][once]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);

    const std::vector<TriggeredEffectConfig_t> effects = {
        OnceEffect_(ModifyPopulationEffect_t{1, ModifierOp_t::Add, 0}, OnceScope_t::Base, "gift"),
        OnceEffect_(ModifyPopulationEffect_t{1, ModifierOp_t::Add, 0}, OnceScope_t::Base, "gift"),
    };

    TriggeredEffectContext_t context(*game.pState, base);
    CHECK(ApplyTriggeredEffects(effects, context).size() == 1);
}

// A unit-scoped key needs a unit in the context; a trigger that has none cannot record the
// key, so the entry is skipped rather than firing forever.
TEST_CASE("A unit-scoped oncePer is skipped when the trigger has no unit",
          "[effects][triggered][once]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);

    const std::vector<TriggeredEffectConfig_t> effects = {
        OnceEffect_(GrantTechEffect_t{"some_tech"}, OnceScope_t::Unit, "monolith_xp"),
    };

    TriggeredEffectContext_t context(*game.pState, base);
    CHECK(ApplyTriggeredEffects(effects, context).empty());
    CHECK_FALSE(game.pFaction->GetResearch().HasDiscoveredTech("some_tech"));
}

// Two facilities may grant the same tech; the second is an ordinary outcome, not the
// programmer error ResearchManager::AddDiscoveredTech throws on.
TEST_CASE("Granting an already-discovered tech is a no-op, not a throw", "[effects][triggered]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    const std::vector<TriggeredEffectConfig_t> effects = {Effect_(GrantTechEffect_t{"some_tech"})};

    TriggeredEffectContext_t context(*game.pState, base);
    CHECK(ApplyTriggeredEffects(effects, context).size() == 1);
    CHECK_NOTHROW(ApplyTriggeredEffects(effects, context));
    CHECK(ApplyTriggeredEffects(effects, context).empty());
    CHECK(game.pFaction->GetResearch().HasDiscoveredTech("some_tech"));
}

// A key is spent by an effect that did something, not by one that found nothing to do —
// otherwise a monolith visited with a full map would burn the gift it never handed over.
TEST_CASE("oncePer is not spent when the entry found nothing to do",
          "[effects][triggered][once]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    // No facility to destroy yet, so the entry reports an empty list and stays unspent.
    const std::vector<TriggeredEffectConfig_t> effects = {
        OnceEffect_(DestroyFacilityEffect_t{1, true, false}, OnceScope_t::Base, "sabotage"),
    };

    TriggeredEffectContext_t context(*game.pState, base);
    const std::vector<TriggeredEffectResult_t> first = ApplyTriggeredEffects(effects, context);
    REQUIRE(first.size() == 1);
    CHECK(std::get<FacilitiesDestroyed_t>(first[0]).buildingIds.empty());

    base.GetBuildingManager().AddBuilding("flat_nutrient");
    const std::vector<TriggeredEffectResult_t> second = ApplyTriggeredEffects(effects, context);
    REQUIRE(second.size() == 1);
    CHECK(std::get<FacilitiesDestroyed_t>(second[0]).buildingIds
          == std::vector<BuildingId_t>{"flat_nutrient"});

    // Now it has been used up.
    CHECK(ApplyTriggeredEffects(effects, context).empty());
}

TEST_CASE("GrantUnit spawns at the context base and homes the unit there",
          "[effects][triggered][grantunit]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);
    REQUIRE(game.pFaction->GetUnitManager().Units().empty());

    const std::vector<TriggeredEffectConfig_t> grant = GrantList_();
    TriggeredEffectContext_t context(*game.pState, base);
    const std::vector<TriggeredEffectResult_t> results = ApplyTriggeredEffects(grant, context);

    REQUIRE(results.size() == 1);
    const auto& granted = std::get<UnitsGranted_t>(results[0]);
    CHECK(granted.count == 1);
    CHECK_FALSE(granted.designId.empty());

    REQUIRE(UnitCount_(*game.pFaction) == 1);
    const Unit& rUnit = FirstUnit_(*game.pFaction);
    CHECK(&rUnit.GetTile() == &base.GetTile());
    CHECK(rUnit.GetHomeBase() == &base);
    // Homed, but not built anywhere: a gift collects no ProducedAtThisBase train bonus and
    // no prototype latch, the same way an escape pod does not.
    CHECK(rUnit.GetProducedAtBase() == nullptr);
    CHECK_FALSE(rUnit.IsPrototype());
    // The ad-hoc design is registered, so a second grant of the same components reuses it.
    CHECK(game.pFaction->GetMilitary().GetDesign(granted.designId) != nullptr);
}

TEST_CASE("GrantUnit spills onto a neighbouring tile when the base tile is occupied",
          "[effects][triggered][grantunit]")
{
    TriggerGame_ game;
    // Only meaningful under the single-unit-per-tile rule; with stacking on, the second unit
    // simply joins the first on the base tile.
    game.pState->GetWorldMap().GetUnitPositions().SetSingleUnitPerTile(true);
    BaseManager& base = game.MakeBase(4, 4);

    const std::vector<TriggeredEffectConfig_t> grant = GrantList_();
    TriggeredEffectContext_t context(*game.pState, base);
    ApplyTriggeredEffects(grant, context);
    const std::vector<TriggeredEffectResult_t> results = ApplyTriggeredEffects(grant, context);

    REQUIRE(std::get<UnitsGranted_t>(results[0]).count == 1);
    REQUIRE(UnitCount_(*game.pFaction) == 2);
    const Unit& rSecond = LastUnit_(*game.pFaction);
    CHECK(&rSecond.GetTile() != &base.GetTile());
    // Still homed at the anchor base even though it stands outside it.
    CHECK(rSecond.GetHomeBase() == &base);
}

// The trigger may have no base at all (a council vote). The anchor then falls back through
// the tile, the headquarters, and finally any base the faction holds.
TEST_CASE("GrantUnit resolves an anchor base when the context has none",
          "[effects][triggered][grantunit]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);

    const std::vector<TriggeredEffectConfig_t> grant = GrantList_();
    TriggeredEffectContext_t context(*game.pState, *game.pFaction); // no pBase, no pTile
    const std::vector<TriggeredEffectResult_t> results = ApplyTriggeredEffects(grant, context);

    REQUIRE(std::get<UnitsGranted_t>(results[0]).count == 1);
    REQUIRE(UnitCount_(*game.pFaction) == 1);
    CHECK(FirstUnit_(*game.pFaction).GetHomeBase() == &base);
}

TEST_CASE("GrantUnit grants nothing when the faction holds no base",
          "[effects][triggered][grantunit]")
{
    TriggerGame_ game;

    const std::vector<TriggeredEffectConfig_t> grant = GrantList_();
    TriggeredEffectContext_t context(*game.pState, *game.pFaction);
    const std::vector<TriggeredEffectResult_t> results = ApplyTriggeredEffects(grant, context);

    REQUIRE(results.size() == 1);
    CHECK(std::get<UnitsGranted_t>(results[0]).count == 0);
    CHECK(game.pFaction->GetUnitManager().Units().empty());
}

// A full map grants fewer units rather than throwing, and reports the real count — the caller
// needs to be able to tell the player what actually arrived.
TEST_CASE("GrantUnit reports a short count when there is nowhere left to stand",
          "[effects][triggered][grantunit]")
{
    TriggerGame_ game;
    BaseManager& base = game.MakeBase(4, 4);

    GrantUnitEffect_t many = k_ScoutGrant;
    many.count = 64;

    const std::vector<TriggeredEffectConfig_t> grant = GrantList_(many);
    TriggeredEffectContext_t context(*game.pState, base);
    const std::vector<TriggeredEffectResult_t> results = ApplyTriggeredEffects(grant, context);

    const int spawned = std::get<UnitsGranted_t>(results[0]).count;
    CHECK(spawned > 0);
    CHECK(spawned < many.count);
    CHECK(UnitCount_(*game.pFaction) == spawned);
}
