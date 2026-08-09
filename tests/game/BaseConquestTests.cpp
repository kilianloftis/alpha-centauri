#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/AttackRules.h"
#include "game/units/BaseConquestConfig.h"
#include "game/units/BaseConquestRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/MovementConstants.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <ranges>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void FillLand_(WorldMap& rMap)
{
    for (auto& pTile : rMap.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

void FillWater_(WorldMap& rMap)
{
    for (auto& pTile : rMap.GetTiles())
    {
        pTile->SetElevation(-100);
    }
}

size_t CountUnits_(Faction& rFaction)
{
    return static_cast<size_t>(std::ranges::distance(rFaction.GetUnitManager().Units()));
}

struct ConquestGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    FactionConfig_t playerDefinition;
    FactionConfig_t aiDefinition;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    explicit ConquestGame_(FactionSpecies_t playerSpecies = FactionSpecies_t::Human,
                           FactionSpecies_t aiSpecies = FactionSpecies_t::Human)
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        FillLand_(*pMap);
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, actest::k_TestRngSeed);

        fixtures.dataContext.baseConquestConfig = std::make_unique<BaseConquestConfig_t>();
        fixtures.dataContext.baseConquestConfig->captureFacilitiesDestroyedMin = 1;
        fixtures.dataContext.baseConquestConfig->captureFacilitiesDestroyedMaxPercent = 100;
        fixtures.dataContext.baseConquestConfig->escapeColonyPod.componentIds = {
            "test_chassis", "test_colony_pod"};
        fixtures.dataContext.unitComponentRegistry =
            std::make_unique<UnitComponentRegistry>();
        fixtures.dataContext.unitComponentRegistry->Load(FixturePath("unit_components.json"));

        pState->GetUnitOrderExecutor().SetGameDataContext(fixtures.dataContext);

        playerDefinition = fixtures.factionDefinition;
        playerDefinition.id = "player";
        playerDefinition.identity.species = playerSpecies;
        aiDefinition = fixtures.factionDefinition;
        aiDefinition.id = "ai";
        aiDefinition.identity.species = aiSpecies;

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, playerDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, aiDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, actest::k_TestFactionSeed);

        pPlayer = &pState->AddFaction(std::move(pFactionA));
        pAi = &pState->AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y, int pop = 3)
    {
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            pState->AllocateBaseId(), "TestBase", pTile, fixtures.dataContext,
            pState->GetTileEffects(), pState->GetSecretProjectAvailability());
        REQUIRE(pBase);
        while (pBase->GetPopulation().GetSize() < pop)
        {
            pBase->GetPopulation().AddPop();
        }
        while (pBase->GetPopulation().GetSize() > pop)
        {
            pBase->GetPopulation().RemovePop();
        }
        return *pBase;
    }

    Unit& MakeUnit(Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds,
                   BaseManager* pHomeBase = nullptr)
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        int slotIndex = 0;
        for (const std::string& rId : rComponentIds)
        {
            const UnitComponentConfig_t* pComponent = fixtures.unitComponents.Find(rId);
            REQUIRE(pComponent);
            UnitSlotConfig_t slot;
            slot.id = "slot_" + std::to_string(slotIndex++);
            slot.displayName = slot.id;
            slot.componentType = pComponent->type;
            slot.required = true;
            assigned[slot.id] = pComponent;
            slots.push_back(slot);
        }
        fixtures.designs.emplace_back(slots, assigned);

        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), fixtures.designs.back(),
            pState->GetWorldMap().GetUnitPositions(), *pTile, pHomeBase);
    }
};

} // namespace

TEST_CASE("CanCaptureBase: cannot_capture_bases is the sole veto", "[unit][conquest]")
{
    ConquestGame_ game;
    game.MakeBase(*game.pAi, 5, 4);

    Unit& land = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    Unit& needle = game.MakeUnit(*game.pPlayer, 4, 5,
                                 {"test_fuel_flight_chassis", "test_weapon"});
    Unit& grav = game.MakeUnit(*game.pPlayer, 3, 4, {"test_flight_chassis", "test_weapon"});
    Unit& former = game.MakeUnit(*game.pPlayer, 3, 5, {"test_chassis", "test_terraformer"});

    CHECK(CanCaptureBase(land));
    CHECK_FALSE(CanCaptureBase(needle));
    CHECK(CanCaptureBase(grav));
    CHECK_FALSE(CanCaptureBase(former));
}

TEST_CASE("Sea-base assault: attack needs pods for land; capture is flag-gated",
          "[unit][conquest]")
{
    ConquestGame_ game;
    FillWater_(game.pState->GetWorldMap());
    game.pState->GetWorldMap().GetTile(4, 4)->SetElevation(100);
    game.pState->GetWorldMap().GetTile(4, 5)->SetElevation(100);
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4);

    Unit& land = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    Unit& amph = game.MakeUnit(*game.pPlayer, 4, 5,
                               {"test_chassis", "test_weapon", "test_amphibious"});
    Unit& sea = game.MakeUnit(*game.pPlayer, 3, 4, {"test_sea_chassis", "test_weapon"});

    // Capture itself is flag-gated only; entry/attack use Permission + CanAttackTile.
    CHECK(CanCaptureBase(land));
    CHECK(CanCaptureBase(amph));
    CHECK(CanCaptureBase(sea));

    WorldMap& rMap = game.pState->GetWorldMap();
    CHECK_FALSE(CanAttackTile(land, rBase.GetTile(), rMap));
    CHECK(CanAttackTile(amph, rBase.GetTile(), rMap));
    CHECK(CanAttackTile(sea, rBase.GetTile(), rMap));
}

TEST_CASE("Land without pods cannot attack or enter a sea base", "[unit][conquest][amphibious]")
{
    ConquestGame_ game;
    FillWater_(game.pState->GetWorldMap());
    // Coastal approach: attacker on land adjacent to a sea base.
    game.pState->GetWorldMap().GetTile(4, 4)->SetElevation(100);
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_sea_chassis"}, &rBase);
    Unit& land = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});

    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryAttack(
        land, defender.GetTile()).has_value());

    defender.GetFaction().GetUnitManager().DestroyUnit(defender);
    MoveOrder_t order;
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStep(
        land, rBase.GetTile(), order).bEntered);
    CHECK(game.pAi->GetBaseCount() == 1);
    CHECK(game.pPlayer->GetBaseCount() == 0);
}

TEST_CASE("Pods land can attack then step into a sea base to capture",
          "[unit][conquest][amphibious]")
{
    ConquestGame_ game;
    FillWater_(game.pState->GetWorldMap());
    game.pState->GetWorldMap().GetTile(4, 4)->SetElevation(100);
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_sea_chassis"}, &rBase);
    Unit& amph = game.MakeUnit(*game.pPlayer, 4, 4,
                               {"test_chassis", "test_weapon", "test_amphibious"});

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        amph, defender.GetTile());
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK(game.pAi->GetBaseCount() == 1);
    // Attack spent one move point; the leftover is enough for a separate capture step.
    REQUIRE(amph.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(amph, rBase.GetTile(), order).bEntered);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK(game.pAi->GetBaseCount() == 0);
}

TEST_CASE("Pods land can enter an undefended sea base to capture it",
          "[unit][conquest][amphibious]")
{
    ConquestGame_ game;
    FillWater_(game.pState->GetWorldMap());
    game.pState->GetWorldMap().GetTile(4, 4)->SetElevation(100);
    game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);

    Unit& amph = game.MakeUnit(*game.pPlayer, 4, 4,
                               {"test_chassis", "test_weapon", "test_amphibious"});
    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(
        amph, *game.pState->GetWorldMap().GetTile(5, 4), order).bEntered);

    CHECK(game.pAi->GetBaseCount() == 0);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
}

TEST_CASE("Killing the last defender cuts pop; stepping in captures, destroys facilities, repairs",
          "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/4);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");
    rBase.GetBuildingManager().AddBuilding("test_secret_project");

    Unit& attacker = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    attacker.SetCurrentHp(1);
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_chassis"}, &rBase);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        attacker, defender.GetTile());
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);
    REQUIRE_FALSE(result->bAttackerDestroyed);

    // Last-defender pop only; ownership unchanged until a separate entry order.
    CHECK(game.pAi->GetBaseCount() == 1);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK((*game.pAi->Bases().begin()).GetPopulation().GetSize() == 3);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(attacker, rBase.GetTile(), order).bEntered);

    // Capture pop (-1); non-SP facility destroyed, SP kept.
    CHECK(game.pAi->GetBaseCount() == 0);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    BaseManager& rCaptured = *game.pPlayer->Bases().begin();
    // Capture is an identity-preserving ownership transfer, not destroy+recreate: same
    // BaseManager object and baseId, just rebound to the new owner.
    CHECK(&rCaptured == &rBase);
    CHECK(rCaptured.GetBaseId() == rBase.GetBaseId());
    CHECK(&rCaptured.GetFaction() == game.pPlayer);
    CHECK(rCaptured.GetPopulation().GetSize() == 2);
    REQUIRE(rCaptured.GetBuildingManager().GetBuildings().size() == 1);
    CHECK(rCaptured.GetBuildingManager().GetBuildings().front()->id == "test_secret_project");
    CHECK(attacker.GetCurrentHp() == attacker.GetStat(StatId_t::HitPoints));
}

TEST_CASE("Perimeter Defense skips last-defender pop loss; capture pop applies on entry",
          "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("Perimeter_Defense");

    Unit& attacker = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_chassis"}, &rBase);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        attacker, defender.GetTile());
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK((*game.pAi->Bases().begin()).GetPopulation().GetSize() == 3);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(attacker, rBase.GetTile(), order).bEntered);

    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK((*game.pPlayer->Bases().begin()).GetPopulation().GetSize() == 2);
}

TEST_CASE("Needlejet can clear a garrison but cannot capture on entry", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);
    const BaseId_t baseId = rBase.GetBaseId();

    Unit& attacker =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_fuel_flight_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_chassis"}, &rBase);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        attacker, defender.GetTile());
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    REQUIRE(game.pAi->GetBaseCount() == 1);
    CHECK((*game.pAi->Bases().begin()).GetBaseId() == baseId);
    CHECK((*game.pAi->Bases().begin()).GetPopulation().GetSize() == 2);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    // Needlejet stand-in has AttackingEndsTurn — the strike spends the rest of the turn.
    CHECK(attacker.GetMoveFragmentsRemaining() == 0);

    // Capture veto is independent of the attack spend; restore moves to attempt entry.
    attacker.SetMoveFragmentsRemaining(
        attacker.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint);
    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(attacker, rBase.GetTile(), order).bEntered);
    REQUIRE(game.pAi->GetBaseCount() == 1);
    CHECK((*game.pAi->Bases().begin()).GetBaseId() == baseId);
    CHECK(game.pPlayer->GetBaseCount() == 0);
}

TEST_CASE("Entering an undefended foreign base captures it", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");

    Unit& mover = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(
        mover, *game.pState->GetWorldMap().GetTile(5, 4), order).bEntered);

    CHECK(game.pAi->GetBaseCount() == 0);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK(&*game.pPlayer->Bases().begin() == &rBase);
    CHECK((*game.pPlayer->Bases().begin()).GetPopulation().GetSize() == 2);
}

TEST_CASE("A raid that consumes the mover ends the move order as UnitDestroyed",
          "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::NativeLife, FactionSpecies_t::Human);
    game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);

    Unit& worm = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "test_psi"});
    const Tile& rBaseTile = *game.pState->GetWorldMap().GetTile(5, 4);
    worm.SetOrder(MoveOrder_t{&rBaseTile});

    // Execute must stop advancing the order rather than touch the freed raider.
    CHECK(game.pState->GetUnitOrderExecutor().Execute(worm) == OrderProgress_t::UnitDestroyed);
    CHECK(CountUnits_(*game.pPlayer) == 0);
}

TEST_CASE("Former cannot capture an undefended base on entry", "[unit][conquest]")
{
    ConquestGame_ game;
    game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);

    Unit& former = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_terraformer"});
    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(
        former, *game.pState->GetWorldMap().GetTile(5, 4), order).bEntered);

    CHECK(game.pAi->GetBaseCount() == 1);
    CHECK(game.pPlayer->GetBaseCount() == 0);
}

TEST_CASE("Native life raids an undefended base then disappears", "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::NativeLife, FactionSpecies_t::Human);
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");

    Unit& worm = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "test_psi"});
    const size_t nativesBefore = CountUnits_(*game.pPlayer);

    MoveOrder_t order;
    const StepResult_t stepped = game.pState->GetUnitOrderExecutor().TryStep(
        worm, *game.pState->GetWorldMap().GetTile(5, 4), order);
    REQUIRE(stepped.bEntered);
    // The raid consumes the worm: the step must say so, or callers keep using freed memory.
    REQUIRE(stepped.bMoverDestroyed);

    CHECK(game.pAi->GetBaseCount() == 1);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK(CountUnits_(*game.pPlayer) == nativesBefore - 1);

    const BaseManager& rRaided = *game.pAi->Bases().begin();
    const bool bFacilityGone = rRaided.GetBuildingManager().GetBuildings().empty();
    const bool bPopLost = rRaided.GetPopulation().GetSize() == 2;
    CHECK(bFacilityGone != bPopLost);
}

TEST_CASE("Human conquering Progenitor reduces population to one and spawns escape pods",
          "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::Human, FactionSpecies_t::Progenitor);
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/5);

    Unit& attacker = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_chassis"}, &rBase);
    const size_t aiUnitsBefore = CountUnits_(*game.pAi);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        attacker, defender.GetTile());
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    // Last-defender casualty: 5→4; still AI-owned until a separate entry order.
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK((*game.pAi->Bases().begin()).GetPopulation().GetSize() == 4);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(attacker, rBase.GetTile(), order).bEntered);

    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    // Species clash leaves 1.
    CHECK((*game.pPlayer->Bases().begin()).GetPopulation().GetSize() == 1);
    // Half of the removed-at-clash population flee as pods (defender already destroyed).
    CHECK(CountUnits_(*game.pAi) >= aiUnitsBefore - 1);
    CHECK(CountUnits_(*game.pAi) > aiUnitsBefore - 1);
}

TEST_CASE("Population reduced to zero razes the base and tombstones Secret Projects",
          "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/1);
    rBase.GetBuildingManager().AddBuilding("test_secret_project");

    Unit& attacker = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_chassis"}, &rBase);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        attacker, defender.GetTile());
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    CHECK(game.pAi->GetBaseCount() == 0);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK(game.pState->IsSecretProjectDestroyed("test_secret_project"));
    // Unavailable forever, but owned by nobody — the two questions the calculator used to
    // answer with one method, so a razed project read as somebody's.
    CHECK(game.pState->GetSecretProjectAvailability().IsUnavailable("test_secret_project"));
    CHECK_FALSE(
        game.pState->GetSecretProjectAvailability().IsOwnedByAnyFaction("test_secret_project"));
}

TEST_CASE("NoConquestRepair leaves the capturer damaged", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4, /*pop=*/3);

    Unit& attacker = game.MakeUnit(
        *game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "test_no_conquest_repair"});
    attacker.SetCurrentHp(1);
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_chassis"}, &rBase);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        attacker, defender.GetTile());
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.pState->GetUnitOrderExecutor().TryStep(attacker, rBase.GetTile(), order).bEntered);

    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK(attacker.GetCurrentHp() == 1);
}

TEST_CASE("A base that starves to nothing is razed through the conquest pathway",
          "[base][raze]")
{
    // Rule: a base at size zero is destroyed, and there is one raze pathway — the same one
    // conquest uses, so secret-project tombstoning cannot drift between them.
    // See docs/game-rules-decisions.md.
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 4, 4);
    const BaseId_t baseId = rBase.GetBaseId();
    REQUIRE(game.pAi->GetBaseCount() == 1);

    while (rBase.GetPopulation().GetSize() > 0)
    {
        rBase.GetPopulation().RemovePop();
    }

    game.pState->RazeBase(rBase);

    CHECK(game.pAi->GetBaseCount() == 0);
    bool bStillPresent = false;
    for (const BaseManager& rRemaining : game.pAi->Bases())
    {
        bStillPresent = bStillPresent || rRemaining.GetBaseId() == baseId;
    }
    CHECK_FALSE(bStillPresent);
}
