#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/effects/EffectEnums.h"
#include "game/economy/EconomyManager.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/buildings/BuildingRules.h"
#include "game/population/PopulationManager.h"
#include "game/terraform/SurfaceOccupancy.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/combat/AttackRules.h"
#include "game/PlayerInteractionQueue.h"
#include "game/base/BaseConquestCalculator.h"
#include "game/base/BaseConquestConfig.h"
#include "game/base/BaseConquestMutations.h"
#include "game/base/BaseConquestRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/movement/MovementConstants.h"
#include "game/units/UnitMoveExecutor.h"
#include "game/units/UnitAttackExecutor.h"
#include "game/units/UnitComponentRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <variant>
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
    FactionConfig_t playerDefinition;
    FactionConfig_t aiDefinition;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    explicit ConquestGame_(FactionSpecies_t playerSpecies = FactionSpecies_t::Human,
                           FactionSpecies_t aiSpecies = FactionSpecies_t::Human)
    {
        FillLand_(fixtures.map);

        fixtures.dataContext.baseConquestConfig->escapeColonyPod.componentIds = {
            "test_chassis", "test_colony_pod"};

        playerDefinition = fixtures.factionDefinition;
        playerDefinition.id = "player";
        playerDefinition.identity.species = playerSpecies;
        aiDefinition = fixtures.factionDefinition;
        aiDefinition.id = "ai";
        aiDefinition.identity.species = aiSpecies;

        auto pFactionA = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, playerDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, fixtures.state.GetUnitIdAllocator(), actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), false, aiDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, fixtures.state.GetUnitIdAllocator(), actest::k_TestFactionSeed);

        pPlayer = &fixtures.state.AddFaction(std::move(pFactionA));
        pAi = &fixtures.state.AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y, int pop = 3)
    {
        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", pTile,
            fixtures.state.GetTileEffects(), std::nullopt);
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

        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            fixtures.state.AllocateUnitId(), fixtures.designs.back(),
            fixtures.state.GetWorldMap().GetUnitPositions(), *pTile, ac::UnitOrigin::Gift(pHomeBase));
    }
};

} // namespace

TEST_CASE("CanCaptureBase: cannot_capture_bases is the sole veto", "[unit][conquest]")
{
    ConquestGame_ game;
    game.MakeBase(*game.pAi, 9, 9);

    Unit& land = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& needle = game.MakeUnit(*game.pPlayer, 7, 9,
                                 {"test_fuel_flight_chassis", "test_weapon"});
    Unit& grav = game.MakeUnit(*game.pPlayer, 7, 7, {"test_flight_chassis", "test_weapon"});
    Unit& former = game.MakeUnit(*game.pPlayer, 6, 8, {"test_chassis", "test_terraformer"});

    CHECK(CanCaptureBase(land));
    CHECK_FALSE(CanCaptureBase(needle));
    CHECK(CanCaptureBase(grav));
    CHECK_FALSE(CanCaptureBase(former));
}

TEST_CASE("Sea-base assault: attack needs pods for land; capture is flag-gated",
          "[unit][conquest]")
{
    ConquestGame_ game;
    FillWater_(game.fixtures.state.GetWorldMap());
    game.fixtures.state.GetWorldMap().GetTile(8, 8)->SetElevation(100);
    game.fixtures.state.GetWorldMap().GetTile(7, 9)->SetElevation(100);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9);

    Unit& land = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& amph = game.MakeUnit(*game.pPlayer, 7, 9,
                               {"test_chassis", "test_weapon", "test_amphibious"});
    Unit& sea = game.MakeUnit(*game.pPlayer, 7, 7, {"test_sea_chassis", "test_weapon"});

    // Capture itself is flag-gated only; entry/attack use enter + attack_unit grids.
    CHECK(CanCaptureBase(land));
    CHECK(CanCaptureBase(amph));
    CHECK(CanCaptureBase(sea));

    CHECK_FALSE(CanAttackTile(land, rBase.GetTile()));
    CHECK(CanAttackTile(amph, rBase.GetTile()));
    CHECK(CanAttackTile(sea, rBase.GetTile()));
}

TEST_CASE("Land without pods cannot attack or enter a sea base", "[unit][conquest][amphibious]")
{
    ConquestGame_ game;
    FillWater_(game.fixtures.state.GetWorldMap());
    // Coastal approach: attacker on land adjacent to a sea base.
    game.fixtures.state.GetWorldMap().GetTile(8, 8)->SetElevation(100);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_sea_chassis"}, &rBase);
    Unit& land = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});

    CHECK_FALSE(game.fixtures.state.GetUnitAttacks().TryAttack(
        land, defender).has_value());

    defender.GetFaction().GetUnitManager().DestroyUnit(defender);
    MoveOrder_t order;
    CHECK_FALSE(game.fixtures.state.GetUnitMoves().TryStep(
        land, rBase.GetTile(), order).bEntered);
    CHECK(game.pAi->GetBaseCount() == 1);
    CHECK(game.pPlayer->GetBaseCount() == 0);
}

TEST_CASE("Pods land can attack then step into a sea base to capture",
          "[unit][conquest][amphibious]")
{
    ConquestGame_ game;
    FillWater_(game.fixtures.state.GetWorldMap());
    game.fixtures.state.GetWorldMap().GetTile(8, 8)->SetElevation(100);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_sea_chassis"}, &rBase);
    Unit& amph = game.MakeUnit(*game.pPlayer, 8, 8,
                               {"test_chassis", "test_weapon", "test_amphibious"});

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(
        amph, defender);
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK(game.pAi->GetBaseCount() == 1);
    // Attack spent one move point; the leftover is enough for a separate capture step.
    REQUIRE(amph.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(amph, rBase.GetTile(), order).bEntered);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK(game.pAi->GetBaseCount() == 0);
}

TEST_CASE("Pods land can enter an undefended sea base to capture it",
          "[unit][conquest][amphibious]")
{
    ConquestGame_ game;
    FillWater_(game.fixtures.state.GetWorldMap());
    game.fixtures.state.GetWorldMap().GetTile(8, 8)->SetElevation(100);
    game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);

    Unit& amph = game.MakeUnit(*game.pPlayer, 8, 8,
                               {"test_chassis", "test_weapon", "test_amphibious"});
    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(
        amph, *game.fixtures.state.GetWorldMap().GetTile(9, 9), order).bEntered);

    CHECK(game.pAi->GetBaseCount() == 0);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
}

TEST_CASE("A single-use attacker that kills the last defender still cuts pop", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/4);
    Unit& missile =
        game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_single_use_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(missile, defender);
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    // terranx-map.md "Last defender lost": the base loses pop before the missile is spent.
    CHECK(result->bAttackerDestroyed);
    CHECK(game.pPlayer->GetUnitManager().Units().empty());
    CHECK(rBase.GetPopulation().GetSize() == 3);
}

TEST_CASE("Killing the last defender cuts pop; stepping in captures and repairs",
          "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/4);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");
    rBase.GetBuildingManager().AddBuilding("test_secret_project");

    Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    attacker.SetCurrentHp(1);
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(
        attacker, defender);
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);
    REQUIRE_FALSE(result->bAttackerDestroyed);

    // Last-defender pop only; ownership unchanged until a separate entry order.
    CHECK(game.pAi->GetBaseCount() == 1);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK((*game.pAi->Bases().begin()).GetPopulation().GetSize() == 3);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(attacker, rBase.GetTile(), order).bEntered);

    // Capture pop (-1); secret projects stay out of the 1/3 loop.
    CHECK(game.pAi->GetBaseCount() == 0);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    BaseManager& rCaptured = *game.pPlayer->Bases().begin();
    // Capture is an identity-preserving ownership transfer, not destroy+recreate: same
    // BaseManager object and baseId, just rebound to the new owner.
    CHECK(&rCaptured == &rBase);
    CHECK(rCaptured.GetBaseId() == rBase.GetBaseId());
    CHECK(&rCaptured.GetFaction() == game.pPlayer);
    CHECK(rCaptured.GetPopulation().GetSize() == 2);
    CHECK(rCaptured.GetBuildingManager().HasBuilding("test_secret_project"));
    CHECK(attacker.GetCurrentHp() == ResolveStat(attacker, StatId_t::HitPoints));
}

TEST_CASE("Perimeter Defense MaxClamp 0 skips last-defender loss; capture loss still applies",
          "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("Perimeter_Defense");

    Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(
        attacker, defender);
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK((*game.pAi->Bases().begin()).GetPopulation().GetSize() == 3);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(attacker, rBase.GetTile(), order).bEntered);

    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK((*game.pPlayer->Bases().begin()).GetPopulation().GetSize() == 2);
}

TEST_CASE("LastDefenderPopLoss MaxClamp leaves CapturePopLoss untouched", "[unit][conquest]")
{
    ConquestGame_ game;
    actest::SetBaseConquestStat(*game.fixtures.dataContext.baseConquestConfig,
                                StatId_t::LastDefenderPopLoss, 3);
    actest::SetBaseConquestStat(*game.fixtures.dataContext.baseConquestConfig,
                                StatId_t::CapturePopLoss, 3);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/6);
    rBase.GetBuildingManager().AddBuilding("test_last_defender_pop_loss_cap");

    Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(
        attacker, defender);
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    CHECK((*game.pAi->Bases().begin()).GetPopulation().GetSize() == 5);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(attacker, rBase.GetTile(), order).bEntered);

    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK((*game.pPlayer->Bases().begin()).GetPopulation().GetSize() == 2);
}

TEST_CASE("Needlejet can clear a garrison but cannot capture on entry", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    const BaseId_t baseId = rBase.GetBaseId();

    Unit& attacker =
        game.MakeUnit(*game.pPlayer, 8, 8, {"test_fuel_flight_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(
        attacker, defender);
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
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(attacker, rBase.GetTile(), order).bEntered);
    REQUIRE(game.pAi->GetBaseCount() == 1);
    CHECK((*game.pAi->Bases().begin()).GetBaseId() == baseId);
    CHECK(game.pPlayer->GetBaseCount() == 0);
}

TEST_CASE("Entering an undefended foreign base captures it", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");

    Unit& mover = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(
        mover, *game.fixtures.state.GetWorldMap().GetTile(9, 9), order).bEntered);

    CHECK(game.pAi->GetBaseCount() == 0);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK(&*game.pPlayer->Bases().begin() == &rBase);
    CHECK((*game.pPlayer->Bases().begin()).GetPopulation().GetSize() == 2);
}

TEST_CASE("A raid that consumes the mover ends the move order as UnitDestroyed",
          "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::NativeLife, FactionSpecies_t::Human);
    game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);

    Unit& worm = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon", "test_psi"});
    const Tile& rBaseTile = *game.fixtures.state.GetWorldMap().GetTile(9, 9);
    worm.SetOrder(MoveOrder_t{&rBaseTile});

    // Execute must stop advancing the order rather than touch the freed raider.
    CHECK(game.fixtures.state.GetUnitOrderExecutor().Execute(worm) == OrderProgress_t::UnitDestroyed);
    CHECK(CountUnits_(*game.pPlayer) == 0);
}

TEST_CASE("Former cannot capture an undefended base on entry", "[unit][conquest]")
{
    ConquestGame_ game;
    game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);

    Unit& former = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_terraformer"});
    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(
        former, *game.fixtures.state.GetWorldMap().GetTile(9, 9), order).bEntered);

    CHECK(game.pAi->GetBaseCount() == 1);
    CHECK(game.pPlayer->GetBaseCount() == 0);
}

TEST_CASE("Native life raids an undefended base then disappears", "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::NativeLife, FactionSpecies_t::Human);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("Headquarters");
    rBase.GetBuildingManager().AddBuilding("test_facility_a");

    Unit& worm = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon", "test_psi"});
    const size_t nativesBefore = CountUnits_(*game.pPlayer);

    MoveOrder_t order;
    const StepResult_t stepped = game.fixtures.state.GetUnitMoves().TryStep(
        worm, *game.fixtures.state.GetWorldMap().GetTile(9, 9), order);
    REQUIRE(stepped.bEntered);
    // The raid consumes the worm: the step must say so, or callers keep using freed memory.
    REQUIRE(stepped.bMoverDestroyed);

    CHECK(game.pAi->GetBaseCount() == 1);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK(CountUnits_(*game.pPlayer) == nativesBefore - 1);

    const BaseManager& rRaided = *game.pAi->Bases().begin();
    // The Headquarters makes two destroyable facilities. Destroying one leaves a building;
    // emptying the list is no longer the facility outcome.
    const bool bFacilityDestroyed = rRaided.GetBuildingManager().GetBuildings().size() == 1;
    const bool bPopLost = rRaided.GetPopulation().GetSize() == 2;
    CHECK(bFacilityDestroyed != bPopLost);
}

TEST_CASE("Human conquering Progenitor reduces population to one and spawns escape pods",
          "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::Human, FactionSpecies_t::Progenitor);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/5);

    Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);
    const size_t aiUnitsBefore = CountUnits_(*game.pAi);

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(
        attacker, defender);
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    // Last-defender casualty: 5→4; still AI-owned until a separate entry order.
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK((*game.pAi->Bases().begin()).GetPopulation().GetSize() == 4);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(attacker, rBase.GetTile(), order).bEntered);

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
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/1);
    rBase.GetBuildingManager().AddBuilding("test_secret_project");

    Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(
        attacker, defender);
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);

    CHECK(game.pAi->GetBaseCount() == 0);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    CHECK(game.fixtures.state.IsSecretProjectDestroyed("test_secret_project"));
    // Unavailable forever, but owned by nobody — the two questions the calculator used to
    // answer with one method, so a razed project read as somebody's.
    CHECK(IsSecretProjectUnavailable("test_secret_project"));
    CHECK_FALSE(IsSecretProjectOwned("test_secret_project"));
}

TEST_CASE("NoConquestRepair leaves the capturer damaged", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);

    Unit& attacker = game.MakeUnit(
        *game.pPlayer, 8, 8, {"test_chassis", "test_weapon", "test_no_conquest_repair"});
    attacker.SetCurrentHp(1);
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);

    auto result = game.fixtures.state.GetUnitAttacks().TryAttack(
        attacker, defender);
    REQUIRE(result.has_value());
    REQUIRE(result->bDefenderDestroyed);
    REQUIRE(attacker.GetMoveFragmentsRemaining() > 0);

    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(attacker, rBase.GetTile(), order).bEntered);

    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    CHECK(attacker.GetCurrentHp() == 1);
}

TEST_CASE("Losing the last pop razes a base where it happens, with no explicit raze call",
          "[base][raze]")
{
    // Rule: a base at size zero leaves the game the instant it is emptied, whatever emptied it
    // — starvation, a production pop cost, genetic plague, conquest. One pathway (the pop-loss
    // handler), so secret-project tombstoning cannot drift between them.
    // See docs/game-rules-decisions.md.
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 8, 8);
    const BaseId_t baseId = rBase.GetBaseId();
    REQUIRE(game.pAi->GetBaseCount() == 1);

    while (rBase.GetPopulation().GetSize() > 0)
    {
        rBase.GetPopulation().RemovePop();
    }

    CHECK(rBase.IsRazed());
    CHECK(game.pAi->GetBaseCount() == 0);
    CHECK(game.pAi->FindBase(baseId) == nullptr);
    bool bStillPresent = false;
    for (const BaseManager& rRemaining : game.pAi->Bases())
    {
        bStillPresent = bStillPresent || rRemaining.GetBaseId() == baseId;
    }
    CHECK_FALSE(bStillPresent);
}

TEST_CASE("A razed base outlives the raze until it is reaped", "[base][raze]")
{
    // The whole point of marking rather than erasing: a base can die from inside its own signal
    // handler, or mid-iteration, without the object going away under the caller.
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 8, 8);

    bool bDestroyedFired = false;
    Signal<>::ScopedConnection connection =
        rBase.OnDestroyed.ConnectScoped([&bDestroyedFired]() { bDestroyedFired = true; });

    while (rBase.GetPopulation().GetSize() > 0)
    {
        rBase.GetPopulation().RemovePop();
    }

    // OnDestroyed fires at the raze, not at the reap, and the object is still readable here.
    CHECK(bDestroyedFired);
    CHECK(rBase.GetBaseId() != 0);

    game.fixtures.state.ReapRazedBases();
    CHECK(game.pAi->GetBaseCount() == 0);
}

TEST_CASE("Capturing a base starts the recently-conquered drone window", "[unit][conquest][drones]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    CHECK_FALSE(rBase.GetPopulation().GetAssimilation().OccupierWindow().IsActive());

    Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    MoveOrder_t order;
    REQUIRE(game.fixtures.state.GetUnitMoves().TryStep(attacker, rBase.GetTile(), order).bEntered);

    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    BaseManager& rCaptured = *game.pPlayer->Bases().begin();
    const AssimilationTracker& rAssim = rCaptured.GetPopulation().GetAssimilation();
    CHECK(rAssim.OccupierWindow().IsActive());
    CHECK(rAssim.OccupierWindow().formerOwner == game.pAi->GetFactionId());
    CHECK(rAssim.OccupierWindow().peakDrones == 5);
    CHECK(rAssim.OccupierWindow().durationTurns == 50);
    CHECK(rAssim.OccupierWindow().turnsElapsed == 0);
}

TEST_CASE("Diplomatic transfer does not start assimilation", "[unit][conquest][drones]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    game.pAi->TransferBaseTo(rBase.GetBaseId(), *game.pPlayer);
    CHECK_FALSE(rBase.GetPopulation().GetAssimilation().OccupierWindow().IsActive());
}

TEST_CASE("Recapture by the former owner reverses remaining assimilation",
          "[unit][conquest][drones]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    PopulationManager& rPops = rBase.GetPopulation();

    rPops.NotifyCaptured(game.pAi->GetFactionId(), game.pPlayer->GetFactionId());
    game.pAi->TransferBaseTo(rBase.GetBaseId(), *game.pPlayer);
    for (int i = 0; i < 12; ++i)
    {
        rPops.AdvanceAssimilation();
    }
    CHECK(rPops.GetAssimilation().OccupierWindow().turnsElapsed == 12);

    rPops.NotifyCaptured(game.pPlayer->GetFactionId(), game.pAi->GetFactionId());
    game.pPlayer->TransferBaseTo(rBase.GetBaseId(), *game.pAi);
    const AssimilationTracker& rAssim = rPops.GetAssimilation();
    CHECK(rAssim.OccupierWindow().IsActive());
    CHECK(rAssim.OccupierWindow().formerOwner == game.pPlayer->GetFactionId());
    CHECK(rAssim.OccupierWindow().peakDrones == 1);
    CHECK(rAssim.OccupierWindow().durationTurns == 12);
    CHECK(rAssim.OccupierWindow().turnsElapsed == 0);

    for (int i = 0; i < 12; ++i)
    {
        rPops.AdvanceAssimilation();
    }
    CHECK_FALSE(rPops.GetAssimilation().OccupierWindow().IsActive());
}

TEST_CASE("Recapture within ten turns clears the penalty", "[unit][conquest][drones]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    PopulationManager& rPops = rBase.GetPopulation();

    rPops.NotifyCaptured(game.pAi->GetFactionId(), game.pPlayer->GetFactionId());
    for (int i = 0; i < 5; ++i)
    {
        rPops.AdvanceAssimilation();
    }
    rPops.NotifyCaptured(game.pPlayer->GetFactionId(), game.pAi->GetFactionId());
    CHECK_FALSE(rPops.GetAssimilation().OccupierWindow().IsActive());
}

TEST_CASE("A third faction capturing during assimilation starts a fresh occupier window",
          "[unit][conquest][drones]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    PopulationManager& rPops = rBase.GetPopulation();
    rPops.NotifyCaptured(game.pAi->GetFactionId(), game.pPlayer->GetFactionId());
    for (int i = 0; i < 12; ++i)
    {
        rPops.AdvanceAssimilation();
    }

    const FactionId_t third = game.pPlayer->GetFactionId() + game.pAi->GetFactionId() + 1;
    rPops.NotifyCaptured(game.pPlayer->GetFactionId(), third);
    const AssimilationState& rWindow = rPops.GetAssimilation().OccupierWindow();
    CHECK(rWindow.formerOwner == game.pPlayer->GetFactionId());
    CHECK(rWindow.peakDrones == 5);
    CHECK(rWindow.durationTurns == 50);
    CHECK(rWindow.turnsElapsed == 0);
}

TEST_CASE("The original owner recapturing after a third party gets the reversed penalty",
          "[unit][conquest][drones]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    PopulationManager& rPops = rBase.GetPopulation();
    const FactionId_t original = game.pAi->GetFactionId();
    const FactionId_t firstCapturer = game.pPlayer->GetFactionId();
    const FactionId_t third = firstCapturer + original + 1;

    rPops.NotifyCaptured(original, firstCapturer);
    for (int i = 0; i < 12; ++i)
    {
        rPops.AdvanceAssimilation();
    }
    rPops.NotifyCaptured(firstCapturer, third);

    rPops.NotifyCaptured(third, original);
    const AssimilationTracker& rAssim = rPops.GetAssimilation();
    CHECK(rAssim.OccupierWindow().IsActive());
    CHECK(rAssim.OccupierWindow().peakDrones == 1);
    CHECK(rAssim.OccupierWindow().durationTurns == 12);
    CHECK(rAssim.OccupierWindow().turnsElapsed == 0);
    CHECK(rAssim.OccupierWindow().formerOwner == third);
}

TEST_CASE("Assimilation expires after fifty turns and the next capture is fresh",
          "[unit][conquest][drones]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    PopulationManager& rPops = rBase.GetPopulation();
    rPops.NotifyCaptured(game.pAi->GetFactionId(), game.pPlayer->GetFactionId());
    for (int i = 0; i < 50; ++i)
    {
        rPops.AdvanceAssimilation();
    }
    CHECK_FALSE(rPops.GetAssimilation().OccupierWindow().IsActive());

    rPops.NotifyCaptured(game.pPlayer->GetFactionId(), game.pAi->GetFactionId());
    const AssimilationState& rWindow = rPops.GetAssimilation().OccupierWindow();
    CHECK(rWindow.formerOwner == game.pPlayer->GetFactionId());
    CHECK(rWindow.peakDrones == 5);
    CHECK(rWindow.durationTurns == 50);
}

TEST_CASE("Planning a cross-species capture keeps one pop and sends half the rest as pods",
          "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::Human, FactionSpecies_t::Progenitor);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/5);
    Unit& attacker = game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "test_weapon"});

    const CaptureOutcome_t outcome = PlanCapture(attacker, rBase);
    CHECK(outcome.popLoss == 4);
    CHECK(outcome.escapePods == 2);
    CHECK(outcome.bRollCaptureFacilityLoss);
    CHECK(rBase.GetPopulation().GetSize() == 5);
    CHECK(ConquestOnEntry(attacker, rBase)
          == ConquestKind_t::Capture);
}

TEST_CASE("A raid on a populated base with a facility is a coin flip between them",
          "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::Human, FactionSpecies_t::Human);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("Headquarters");

    const NativeRaidPlan_t plan = PlanNativeRaid(rBase);
    CHECK_FALSE(plan.facilityCandidates.empty());
    CHECK(plan.facilityChancePercent == 50);
    CHECK(plan.popLoss == 1);
}

TEST_CASE("Killing an undefended base's last defender costs a casualty pop", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9);
    const Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    CHECK(ConquestOnLastDefenderKill(attacker, rBase)
          == ConquestKind_t::LastDefenderCasualty);

    game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"});
    CHECK_FALSE(ConquestOnLastDefenderKill(attacker, rBase).has_value());
}

TEST_CASE("Native life that kills a base's last defender raids it", "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::NativeLife, FactionSpecies_t::Human);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9);
    const Unit& worm =
        game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "native_life_chassis"});
    CHECK(ConquestOnLastDefenderKill(worm, rBase)
          == ConquestKind_t::NativeRaid);
}

TEST_CASE("A raid that misses the facility takes the configured pop loss",
          "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::NativeLife, FactionSpecies_t::Human);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/4);
    game.fixtures.dataContext.baseConquestConfig->nativeRaid.facilityChancePercent = 0;
    game.fixtures.dataContext.baseConquestConfig->nativeRaid.popLoss = 2;

    Unit& worm =
        game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "native_life_chassis"});
    std::mt19937 rng(1);
    const BaseConquestResult_t result =
        ResolveBaseEntryConquest(worm, game.fixtures.state, rng);

    CHECK(result.outcome == BaseConquestOutcome_t::NativeRaid);
    CHECK(result.populationLost == 2);
    CHECK(rBase.GetPopulation().GetSize() == 2);
    CHECK(result.bActorDestroyed);
}

TEST_CASE("ResolvePostCombatBaseConquest applies a native raid after the casualty",
          "[unit][conquest]")
{
    ConquestGame_ game(FactionSpecies_t::NativeLife, FactionSpecies_t::Human);
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("Headquarters");
    game.fixtures.dataContext.baseConquestConfig->nativeRaid.facilityChancePercent = 100;

    Unit& worm =
        game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "native_life_chassis"});
    std::mt19937 rng(1);
    const BaseConquestResult_t result =
        ResolvePostCombatBaseConquest(worm, rBase.GetTile(), game.fixtures.state, rng);

    CHECK(result.outcome == BaseConquestOutcome_t::NativeRaid);
    CHECK(result.bActorDestroyed);
    CHECK(result.facilitiesDestroyed == 1);
    CHECK_FALSE(rBase.GetBuildingManager().HasBuilding("Headquarters"));
}

TEST_CASE("ResolveBaseEntryConquest rejects a GameState that is not current",
          "[unit][conquest][current-game-state]")
{
    ConquestGame_ game;
    Unit& mover = game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "test_weapon"});
    auto pOther = std::make_unique<GameState>(
        std::make_unique<WorldMap>(actest::k_TestMapWidth, actest::k_TestMapHeight,
                                   actest::TestMapRules(), Rainfall_t::Average),
        game.fixtures.dataContext, game.fixtures.settings, actest::k_TestRngSeed);
    std::mt19937 rng(1);
    CHECK_THROWS_AS(ResolveBaseEntryConquest(mover, *pOther, rng), std::logic_error);
}

namespace
{

// rCaptor's unit steps from (8, 8) onto the base at (9, 9).
void CaptureAt99_(ConquestGame_& rGame, Faction& rCaptor)
{
    Unit& rMover = rGame.MakeUnit(rCaptor, 8, 8, {"test_chassis", "test_weapon"});
    MoveOrder_t order;
    Tile& rTarget = *rGame.fixtures.state.GetWorldMap().GetTile(9, 9);
    REQUIRE(rGame.fixtures.state.GetUnitMoves().TryStep(rMover, rTarget, order).bEntered);
}

// An owner with a Headquarters at (9, 9) and three more bases on row 13; (17, 13) sits between
// the other two, so it is the escape.
struct EscapeSetup_t
{
    BaseManager* pHeadquarters = nullptr;
    BaseManager* pWest = nullptr;
    BaseManager* pMiddle = nullptr;
    BaseManager* pEast = nullptr;
};

EscapeSetup_t MakeEscapeSetup_(ConquestGame_& rGame, Faction& rOwner)
{
    EscapeSetup_t setup;
    setup.pHeadquarters = &rGame.MakeBase(rOwner, 9, 9);
    setup.pHeadquarters->GetBuildingManager().AddBuilding("Headquarters");
    setup.pWest = &rGame.MakeBase(rOwner, 13, 13);
    setup.pMiddle = &rGame.MakeBase(rOwner, 17, 13);
    setup.pEast = &rGame.MakeBase(rOwner, 27, 13);
    return setup;
}

int EscapeCost_(ConquestGame_& rGame)
{
    return rGame.fixtures.dataContext.baseConquestConfig->headquartersEscape.cost;
}

bool QueuedEscapeFor_(ConquestGame_& rGame, const Faction& rOwner, const BaseManager& rTo)
{
    return rGame.fixtures.state.GetPlayerInteractions().AnyOf(
        [&](const QueuedInteraction_t& rQueued)
        {
            const auto* pEscape = std::get_if<HeadquartersEscapeInteraction_t>(&rQueued.payload);
            return pEscape && pEscape->ownerId == rOwner.GetFactionId()
                && pEscape->destinationId == rTo.GetBaseId();
        });
}

} // namespace

TEST_CASE("A captured Headquarters is always lost, whatever the random facility loss",
          "[unit][conquest][hq]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9);
    rBase.GetBuildingManager().AddBuilding("Headquarters");

    CaptureAt99_(game, *game.pPlayer);

    REQUIRE(&rBase.GetFaction() == game.pPlayer);
    CHECK_FALSE(rBase.IsHeadquarters());
    CHECK(game.pPlayer->GetHeadquarters() == nullptr);
}

TEST_CASE("Capture shuffle-takes the resolved facility count then wipes Commons",
          "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");
    rBase.GetBuildingManager().AddBuilding("test_facility_b");
    rBase.GetBuildingManager().AddBuilding("Recreation_Commons");
    Unit& capturer = game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "test_weapon"});

    const CaptureOutcome_t plan = PlanCapture(capturer, rBase);
    REQUIRE(plan.bRollCaptureFacilityLoss);
    REQUIRE(plan.facilityCandidates.size() == 3);
    // Eligible 3 × MultiplyGeometric 1/3 → 1.
    CHECK(plan.facilitiesToDestroy == 1);

    // Fixed seed: the same shuffle DestroyRandomFacilities_ uses, then Commons wipe if it
    // survived the take-N.
    constexpr uint32_t kSeed = 42;
    std::vector<const BuildingConfig_t*> shuffled = plan.facilityCandidates;
    std::mt19937 expectedRng(kSeed);
    std::shuffle(shuffled.begin(), shuffled.end(), expectedRng);
    int expectedDestroyed = plan.facilitiesToDestroy;
    bool bCommonsTaken = false;
    for (int i = 0; i < plan.facilitiesToDestroy; ++i)
    {
        if (shuffled[static_cast<size_t>(i)]->id == "Recreation_Commons")
        {
            bCommonsTaken = true;
        }
    }
    if (!bCommonsTaken)
    {
        ++expectedDestroyed;
    }

    std::mt19937 rng(kSeed);
    const BaseConquestResult_t result =
        ResolveBaseEntryConquest(capturer, game.fixtures.state, rng);
    CHECK(result.outcome == BaseConquestOutcome_t::Captured);
    CHECK(result.facilitiesDestroyed == expectedDestroyed);
    CHECK_FALSE(rBase.GetBuildingManager().HasBuilding("Recreation_Commons"));
}

TEST_CASE("A facility can MaxClamp capture facility destruction to nothing", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");
    rBase.GetBuildingManager().AddBuilding("test_facility_b");
    rBase.GetBuildingManager().AddBuilding("Recreation_Commons");
    rBase.GetBuildingManager().AddBuilding("test_capture_facility_hardening");
    Unit& capturer = game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "test_weapon"});

    const CaptureOutcome_t plan = PlanCapture(capturer, rBase);
    REQUIRE(plan.bRollCaptureFacilityLoss);
    CHECK(plan.facilitiesToDestroy == 0);

    std::mt19937 rng(1);
    const BaseConquestResult_t result =
        ResolveBaseEntryConquest(capturer, game.fixtures.state, rng);
    CHECK(result.outcome == BaseConquestOutcome_t::Captured);
    // Commons/Tanks wipe still runs when rolling; only the shuffle-take is zeroed.
    CHECK_FALSE(rBase.GetBuildingManager().HasBuilding("Recreation_Commons"));
    CHECK(rBase.GetBuildingManager().HasBuilding("test_facility_a"));
    CHECK(rBase.GetBuildingManager().HasBuilding("test_facility_b"));
    CHECK(rBase.GetBuildingManager().HasBuilding("test_capture_facility_hardening"));
}

TEST_CASE("Pressure Dome on a sea base survives capture's random facility loss",
          "[unit][conquest]")
{
    ConquestGame_ game;
    FillWater_(game.fixtures.state.GetWorldMap());
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding(k_PressureDomeBuildingId);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");
    Unit& capturer =
        game.MakeUnit(*game.pPlayer, 9, 9, {"test_sea_chassis", "test_weapon"});

    const CaptureOutcome_t plan = PlanCapture(capturer, rBase);
    for (const BuildingConfig_t* pBuilding : plan.facilityCandidates)
    {
        CHECK(pBuilding->id != k_PressureDomeBuildingId);
    }

    std::mt19937 rng(1);
    ResolveBaseEntryConquest(capturer, game.fixtures.state, rng);
    CHECK(rBase.GetBuildingManager().HasBuilding(k_PressureDomeBuildingId));
}

TEST_CASE("Liberation by the former owner skips capture facility loss", "[unit][conquest]")
{
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pPlayer, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("test_facility_a");
    rBase.GetBuildingManager().AddBuilding("test_facility_b");
    rBase.GetBuildingManager().AddBuilding("Recreation_Commons");

    // Ai takes the player's base; OccupierWindow.formerOwner is the player.
    rBase.GetPopulation().NotifyCaptured(game.pPlayer->GetFactionId(), game.pAi->GetFactionId());
    game.pPlayer->TransferBaseTo(rBase.GetBaseId(), *game.pAi);
    REQUIRE(rBase.GetPopulation().GetAssimilation().OccupierWindow().formerOwner
            == game.pPlayer->GetFactionId());

    Unit& liberator = game.MakeUnit(*game.pPlayer, 9, 9, {"test_chassis", "test_weapon"});
    const CaptureOutcome_t plan = PlanCapture(liberator, rBase);
    CHECK_FALSE(plan.bRollCaptureFacilityLoss);

    std::mt19937 rng(1);
    const BaseConquestResult_t result =
        ResolveBaseEntryConquest(liberator, game.fixtures.state, rng);
    CHECK(result.outcome == BaseConquestOutcome_t::Captured);
    CHECK(result.facilitiesDestroyed == 0);
    CHECK(rBase.GetBuildingManager().HasBuilding("test_facility_a"));
    CHECK(rBase.GetBuildingManager().HasBuilding("test_facility_b"));
    CHECK(rBase.GetBuildingManager().HasBuilding("Recreation_Commons"));
}

TEST_CASE("TakeBase drops Headquarters but leaves other facilities", "[unit][conquest]")
{
    // Mind control calls TakeBase and never ApplyCapture_'s facility rolls.
    ConquestGame_ game;
    BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9, /*pop=*/3);
    rBase.GetBuildingManager().AddBuilding("Headquarters");
    rBase.GetBuildingManager().AddBuilding("test_facility_a");
    rBase.GetBuildingManager().AddBuilding("Recreation_Commons");

    TakeBase(game.fixtures.state, rBase, *game.pPlayer);

    CHECK_FALSE(rBase.IsHeadquarters());
    CHECK(rBase.GetBuildingManager().HasBuilding("test_facility_a"));
    CHECK(rBase.GetBuildingManager().HasBuilding("Recreation_Commons"));
}

TEST_CASE("The escape goes to the base closest to the owner's others", "[unit][conquest][hq]")
{
    ConquestGame_ game;
    const EscapeSetup_t setup = MakeEscapeSetup_(game, *game.pAi);
    game.pAi->GetEconomy().AddEnergy(EscapeCost_(game));

    CHECK(HeadquartersEscapeDestination(*setup.pHeadquarters) == setup.pMiddle->GetBaseId());
    CHECK_FALSE(HeadquartersEscapeDestination(*setup.pWest));
}

TEST_CASE("At equal spread the escape goes to the more populous base", "[unit][conquest][hq]")
{
    ConquestGame_ game;
    BaseManager& rHeadquarters = game.MakeBase(*game.pAi, 9, 9);
    rHeadquarters.GetBuildingManager().AddBuilding("Headquarters");
    game.MakeBase(*game.pAi, 13, 13, /*pop=*/3);
    BaseManager& rLarger = game.MakeBase(*game.pAi, 27, 13, /*pop=*/6);
    game.pAi->GetEconomy().AddEnergy(EscapeCost_(game));

    CHECK(HeadquartersEscapeDestination(rHeadquarters) == rLarger.GetBaseId());
}

TEST_CASE("An AI that can pay moves its captured Headquarters and the player hears of it",
          "[unit][conquest][hq]")
{
    ConquestGame_ game;
    const EscapeSetup_t setup = MakeEscapeSetup_(game, *game.pAi);
    game.pAi->GetEconomy().AddEnergy(EscapeCost_(game) + 7);

    CaptureAt99_(game, *game.pPlayer);

    CHECK(game.pAi->GetHeadquarters() == setup.pMiddle);
    CHECK(game.pAi->GetEconomy().GetEnergy() == 7);
    CHECK_FALSE(setup.pHeadquarters->IsHeadquarters());
    CHECK(QueuedEscapeFor_(game, *game.pAi, *setup.pMiddle));
}

TEST_CASE("Without the cost a captured Headquarters cannot escape", "[unit][conquest][hq]")
{
    ConquestGame_ game;
    MakeEscapeSetup_(game, *game.pAi);
    game.pAi->GetEconomy().AddEnergy(EscapeCost_(game) - 1);

    CaptureAt99_(game, *game.pPlayer);

    CHECK(game.pAi->GetHeadquarters() == nullptr);
    CHECK(game.pAi->GetEconomy().GetEnergy() == EscapeCost_(game) - 1);
    CHECK(game.fixtures.state.GetPlayerInteractions().Empty());
}

TEST_CASE("The player is asked before its captured Headquarters moves", "[unit][conquest][hq]")
{
    ConquestGame_ game;
    const EscapeSetup_t setup = MakeEscapeSetup_(game, *game.pPlayer);
    game.pPlayer->GetEconomy().AddEnergy(EscapeCost_(game));

    CaptureAt99_(game, *game.pAi);

    CHECK(game.pPlayer->GetHeadquarters() == nullptr);
    CHECK(game.pPlayer->GetEconomy().GetEnergy() == EscapeCost_(game));
    REQUIRE(QueuedEscapeFor_(game, *game.pPlayer, *setup.pMiddle));
    REQUIRE(MayEvacuateHeadquarters(*game.pPlayer, *setup.pMiddle));

    EvacuateHeadquarters(game.fixtures.state, *game.pPlayer, *setup.pMiddle);

    CHECK(game.pPlayer->GetHeadquarters() == setup.pMiddle);
    CHECK(game.pPlayer->GetEconomy().GetEnergy() == 0);
    CHECK_FALSE(MayEvacuateHeadquarters(*game.pPlayer, *setup.pEast));
}

TEST_CASE("The escape score weighs spread, region and population",
          "[unit][conquest][hq][calculator]")
{
    ConquestGame_ game;
    const BaseConquestCalculator& rCalculator = *game.fixtures.dataContext.baseConquestCalculator;
    EscapeScoreInputs_t inputs;
    inputs.sameRegionDistance = 10;
    inputs.otherRegionDistance = 5;
    inputs.regionBases = 2;
    inputs.population = 3;

    // (1 + 10 + 2 x 5) x 256 / (2 + 2 x 3), from FactionFixture's headquarters_escape.
    CHECK(rCalculator.HeadquartersEscapeScore(inputs) == 672);
}

