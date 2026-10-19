#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/combat/AttackRules.h"
#include "game/base/BaseConquestConfig.h"
#include "game/movement/MovementRules.h"
#include "game/movement/TransportRules.h"
#include "game/movement/TransportMutations.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void FillWater_(WorldMap& rMap)
{
    for (auto& pTile : rMap.GetTiles())
    {
        pTile->SetElevation(-100);
    }
}

struct AttackGame_
{
    FactionFixture fixtures;
    FactionConfig_t playerDefinition;
    FactionConfig_t aiDefinition;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    AttackGame_()
    {
        FillWater_(fixtures.map);

        playerDefinition = fixtures.factionDefinition;
        playerDefinition.id = "player";
        aiDefinition = fixtures.factionDefinition;
        aiDefinition.id = "ai";

        auto pFactionA = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, playerDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), false, aiDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);
        pPlayer = &fixtures.state.AddFaction(std::move(pFactionA));
        pAi = &fixtures.state.AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y, int pop = 1)
    {
        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", pTile,
            fixtures.state.GetTileEffects());
        REQUIRE(pBase);
        while (pBase->GetPopulation().GetSize() < pop)
        {
            pBase->GetPopulation().AddPop();
        }
        return *pBase;
    }

    Unit& MakeUnit(Faction& rFaction, int x, int y, const std::vector<std::string>& rComponentIds,
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
            fixtures.state.GetWorldMap().GetUnitPositions(), *pTile, pHomeBase);
    }
};

enum class AttackerStance_t
{
    Shore,
    SeaBase,
    Transport,
};

enum class TargetKind_t
{
    EnemySeaBase,
    AdjacentLand,
    OpenWater,
};

struct AttackCase_t
{
    const char* name;
    AttackerStance_t from;
    TargetKind_t target;
    bool bAllowedWithoutPods;
    bool bAllowedWithPods;
};

const AttackCase_t k_AttackMatrix[] = {
    {"shore -> enemy sea base", AttackerStance_t::Shore, TargetKind_t::EnemySeaBase, false, true},
    {"shore -> adjacent land", AttackerStance_t::Shore, TargetKind_t::AdjacentLand, true, true},
    {"shore -> open water", AttackerStance_t::Shore, TargetKind_t::OpenWater, false, false},
    {"sea base -> enemy sea base", AttackerStance_t::SeaBase, TargetKind_t::EnemySeaBase, false,
     true},
    {"sea base -> adjacent land", AttackerStance_t::SeaBase, TargetKind_t::AdjacentLand, false,
     true},
    {"sea base -> open water", AttackerStance_t::SeaBase, TargetKind_t::OpenWater, false, false},
    {"transport -> enemy sea base", AttackerStance_t::Transport, TargetKind_t::EnemySeaBase, false,
     true},
    {"transport -> adjacent land", AttackerStance_t::Transport, TargetKind_t::AdjacentLand, false,
     true},
    {"transport -> open water", AttackerStance_t::Transport, TargetKind_t::OpenWater, false, false},
};

const Tile& PlaceTarget_(AttackGame_& game, TargetKind_t target, int tx, int ty)
{
    WorldMap& rMap = game.fixtures.state.GetWorldMap();
    switch (target)
    {
        case TargetKind_t::EnemySeaBase:
        {
            rMap.GetTile(tx, ty)->SetElevation(-100);
            BaseManager& rBase = game.MakeBase(*game.pAi, tx, ty);
            game.MakeUnit(*game.pAi, tx, ty, {"test_sea_chassis", "test_weapon"}, &rBase);
            return rBase.GetTile();
        }
        case TargetKind_t::AdjacentLand:
            rMap.GetTile(tx, ty)->SetElevation(100);
            return *rMap.GetTile(tx, ty);
        case TargetKind_t::OpenWater:
            rMap.GetTile(tx, ty)->SetElevation(-100);
            game.MakeUnit(*game.pAi, tx, ty, {"test_sea_chassis", "test_weapon"});
            return *rMap.GetTile(tx, ty);
    }
    throw std::runtime_error("unknown target");
}

Unit& PlaceAttacker_(AttackGame_& game, AttackerStance_t stance, bool bPods, int ax, int ay,
                     BaseManager** ppHomeOut = nullptr)
{
    std::vector<std::string> components = {"test_chassis", "test_weapon"};
    if (bPods)
    {
        components.push_back("test_amphibious");
    }
    WorldMap& rMap = game.fixtures.state.GetWorldMap();

    switch (stance)
    {
        case AttackerStance_t::Shore:
            rMap.GetTile(ax, ay)->SetElevation(100);
            return game.MakeUnit(*game.pPlayer, ax, ay, components);
        case AttackerStance_t::SeaBase:
        {
            rMap.GetTile(ax, ay)->SetElevation(-100);
            BaseManager& rHome = game.MakeBase(*game.pPlayer, ax, ay);
            if (ppHomeOut)
            {
                *ppHomeOut = &rHome;
            }
            return game.MakeUnit(*game.pPlayer, ax, ay, components, &rHome);
        }
        case AttackerStance_t::Transport:
        {
            rMap.GetTile(ax, ay)->SetElevation(-100);
            game.MakeUnit(*game.pPlayer, ax, ay, {"test_sea_chassis", "test_transport"});
            Unit& cargo = game.MakeUnit(*game.pPlayer, ax, ay, components);
            REQUIRE(TryAttachToTransport(cargo, rMap));
            return cargo;
        }
    }
    throw std::runtime_error("unknown stance");
}

void CheckMatrixCase_(const AttackCase_t& rCase, bool bPods)
{
    AttackGame_ game;
    // Target at (9, 9); attacker at (8, 8) so they are adjacent.
    const Tile& rTarget = PlaceTarget_(game, rCase.target, 9, 9);
    Unit& rAttacker = PlaceAttacker_(game, rCase.from, bPods, 8, 8);
    const bool bExpected = bPods ? rCase.bAllowedWithPods : rCase.bAllowedWithoutPods;
    CHECK(CanAttackTile(rAttacker, rTarget) == bExpected);
}

} // namespace

TEST_CASE("Land attack matrix: reachability plus the attack_tile grid's footing cell",
          "[unit][attack][amphibious]")
{
    for (const AttackCase_t& rCase : k_AttackMatrix)
    {
        DYNAMIC_SECTION(std::string(rCase.name) + " without pods")
        {
            CheckMatrixCase_(rCase, /*bPods=*/false);
        }
        DYNAMIC_SECTION(std::string(rCase.name) + " with pods")
        {
            CheckMatrixCase_(rCase, /*bPods=*/true);
        }
    }
}

TEST_CASE("CanAttackTile implies CanEnterTile; amphibious assault still needs pods",
          "[unit][attack][coherence]")
{
    AttackGame_ game;
    WorldMap& rMap = game.fixtures.state.GetWorldMap();
    const InteractionGridsConfig_t& rGrids =
        game.fixtures.state.GetTileEffects().GetInteractionGrids();
    rMap.GetTile(8, 8)->SetElevation(100);
    Unit& land = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& pods = game.MakeUnit(*game.pPlayer, 7, 7,
                               {"test_chassis", "test_weapon", "test_amphibious"});

    BaseManager& rEnemy = game.MakeBase(*game.pAi, 9, 9);
    CHECK_FALSE(CanEnterTile(land, rEnemy.GetTile(), rMap, rGrids));
    CHECK_FALSE(CanAttackTile(land, rEnemy.GetTile()));
    CHECK(CanEnterTile(pods, rEnemy.GetTile(), rMap, rGrids));
    CHECK(CanAttackTile(pods, rEnemy.GetTile()));

    rMap.GetTile(7, 7)->SetElevation(100);
    CHECK(CanEnterTile(land, *rMap.GetTile(7, 7), rMap, rGrids));
    CHECK(CanAttackTile(land, *rMap.GetTile(7, 7)));
}

TEST_CASE("All domains: CanAttackTile requires CanEnterTile", "[unit][attack]")
{
    AttackGame_ game;
    WorldMap& rMap = game.fixtures.state.GetWorldMap();
    const InteractionGridsConfig_t& rGrids =
        game.fixtures.state.GetTileEffects().GetInteractionGrids();
    rMap.GetTile(8, 8)->SetElevation(100);
    rMap.GetTile(8, 10)->SetElevation(-100);
    Unit& sea = game.MakeUnit(*game.pPlayer, 8, 10, {"test_sea_chassis", "test_weapon"});
    Unit& air = game.MakeUnit(*game.pPlayer, 8, 8, {"test_flight_chassis", "test_weapon"});

    CHECK_FALSE(CanAttackTile(sea, *rMap.GetTile(8, 8))); // shore
    CHECK(CanAttackTile(sea, *rMap.GetTile(9, 11)));        // open water
    // Air can enter land and water, so both attacks are legal.
    CHECK(CanAttackTile(air, *rMap.GetTile(8, 8)));
    CHECK(CanAttackTile(air, *rMap.GetTile(8, 10)));
    CHECK(CanEnterTile(air, *rMap.GetTile(8, 8), rMap, rGrids));
    CHECK(CanEnterTile(air, *rMap.GetTile(8, 10), rMap, rGrids));
}

TEST_CASE("FindAttackableHostileOnTile matches TryAttack declare gates", "[unit][attack]")
{
    AttackGame_ game;
    WorldMap& rMap = game.fixtures.state.GetWorldMap();
    rMap.GetTile(8, 8)->SetElevation(100);
    rMap.GetTile(9, 9)->SetElevation(100);
    Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis", "test_weapon"});

    Unit* pTarget = FindAttackableHostileOnTile(attacker, defender.GetTile());
    REQUIRE(pTarget == &defender);
    CHECK(CanDeclareAttack(attacker, defender.GetTile()));

    attacker.SetMoveFragmentsRemaining(0);
    CHECK_FALSE(CanDeclareAttack(attacker, defender.GetTile()));
    CHECK(FindVisibleHostileOnTile(attacker, defender.GetTile()) == &defender);
}

TEST_CASE("Embarked cargo defends only in a base; carrier preferred",
          "[unit][attack][defense][transport]")
{
    AttackGame_ game;
    WorldMap& rMap = game.fixtures.state.GetWorldMap();

    SECTION("open-sea transport: cargo not selectable")
    {
        rMap.GetTile(8, 10)->SetElevation(-100);
        rMap.GetTile(7, 9)->SetElevation(100);
        Unit& transport = game.MakeUnit(*game.pAi, 8, 10, {"test_sea_chassis", "test_transport"});
        Unit& cargo = game.MakeUnit(*game.pAi, 8, 10, {"test_chassis"});
        REQUIRE(TryAttachToTransport(cargo, rMap));
        Unit& attacker = game.MakeUnit(*game.pPlayer, 7, 9,
                                       {"test_chassis", "test_weapon", "test_amphibious"});

        Unit* pTarget = FindVisibleHostileOnTile(attacker, *rMap.GetTile(8, 10));
        REQUIRE(pTarget == &transport);
        CHECK(pTarget != &cargo);
        // Shore land without enterability onto open water cannot declare the attack.
        CHECK_FALSE(CanDeclareAttack(attacker, *rMap.GetTile(8, 10)));
    }

    SECTION("base: embarked cargo eligible; carrier preferred")
    {
        BaseManager& rBase = game.MakeBase(*game.pAi, 9, 9);
        Unit& transport =
            game.MakeUnit(*game.pAi, 9, 9, {"test_sea_chassis", "test_transport"}, &rBase);
        Unit& cargo = game.MakeUnit(*game.pAi, 9, 9, {"test_chassis"}, &rBase);
        REQUIRE(TryAttachToTransport(cargo, rMap));
        rMap.GetTile(8, 8)->SetElevation(100);
        Unit& attacker = game.MakeUnit(*game.pPlayer, 8, 8,
                                       {"test_chassis", "test_weapon", "test_amphibious"});

        Unit* pTarget = FindVisibleHostileOnTile(attacker, rBase.GetTile());
        REQUIRE(pTarget == &transport);

        game.pAi->GetUnitManager().DestroyUnit(transport);
        Unit* pAfter = FindVisibleHostileOnTile(attacker, rBase.GetTile());
        REQUIRE(pAfter != nullptr);
        CHECK(pAfter->GetFaction().GetFactionId() == game.pAi->GetFactionId());
    }
}

TEST_CASE("Air and Orbital targets require Air Superiority unless harbored",
          "[unit][attack][air-superiority]")
{
    AttackGame_ game;
    WorldMap& rMap = game.fixtures.state.GetWorldMap();
    rMap.GetTile(8, 8)->SetElevation(100);
    rMap.GetTile(9, 9)->SetElevation(100);
    rMap.GetTile(10, 10)->SetElevation(100);

    Unit& land = game.MakeUnit(*game.pPlayer, 8, 8, {"test_chassis", "test_weapon"});
    Unit& airDefender =
        game.MakeUnit(*game.pAi, 9, 9, {"test_flight_chassis", "test_weapon"});
    Unit& missileDefender =
        game.MakeUnit(*game.pAi, 10, 10, {"test_orbital_chassis", "test_weapon"});

    SECTION("land cannot attack air or orbital without the flag")
    {
        CHECK_FALSE(CanDeclareAttack(land, airDefender.GetTile()));
        Unit& landNearMissile =
            game.MakeUnit(*game.pPlayer, 8, 10, {"test_chassis", "test_weapon"});
        rMap.GetTile(8, 10)->SetElevation(100);
        CHECK_FALSE(CanDeclareAttack(landNearMissile, missileDefender.GetTile()));
    }

    SECTION("Air Superiority unlocks air and orbital targets")
    {
        // (5,5) is adjacent to both (5,4) and (6,4).
        Unit& withFlag = game.MakeUnit(*game.pPlayer, 8, 10,
                                       {"test_chassis", "test_weapon", "air_superiority"});
        rMap.GetTile(8, 10)->SetElevation(100);
        CHECK(CanDeclareAttack(withFlag, airDefender.GetTile()));
        CHECK(CanDeclareAttack(withFlag, missileDefender.GetTile()));
    }

    SECTION("air-to-air and missile-to-air still need the flag")
    {
        Unit& airAttacker =
            game.MakeUnit(*game.pPlayer, 7, 9, {"test_flight_chassis", "test_weapon"});
        Unit& missileAttacker =
            game.MakeUnit(*game.pPlayer, 8, 10, {"test_orbital_chassis", "test_weapon"});
        rMap.GetTile(7, 9)->SetElevation(100);
        rMap.GetTile(8, 10)->SetElevation(100);
        CHECK_FALSE(CanDeclareAttack(airAttacker, airDefender.GetTile()));
        CHECK_FALSE(CanDeclareAttack(missileAttacker, airDefender.GetTile()));

        Unit& flagged = game.MakeUnit(
            *game.pPlayer, 9, 11, {"test_flight_chassis", "test_weapon", "air_superiority"});
        rMap.GetTile(9, 11)->SetElevation(100);
        CHECK(CanDeclareAttack(flagged, airDefender.GetTile()));
    }

    SECTION("harbored base tile exempts the gate")
    {
        rMap.GetTile(8, 10)->SetElevation(100);
        rMap.GetTile(9, 11)->SetElevation(100);
        rMap.GetTile(7, 9)->SetElevation(100);
        rMap.GetTile(7, 11)->SetElevation(100);
        BaseManager& rAirBase = game.MakeBase(*game.pAi, 8, 10);
        BaseManager& rMissileBase = game.MakeBase(*game.pAi, 9, 11);
        Unit& groundedAir =
            game.MakeUnit(*game.pAi, 8, 10, {"test_flight_chassis", "test_weapon"}, &rAirBase);
        Unit& groundedMissile = game.MakeUnit(
            *game.pAi, 9, 11, {"test_orbital_chassis", "test_weapon"}, &rMissileBase);
        Unit& nearAir = game.MakeUnit(*game.pPlayer, 7, 9, {"test_chassis", "test_weapon"});
        Unit& nearMissile = game.MakeUnit(*game.pPlayer, 7, 11, {"test_chassis", "test_weapon"});
        REQUIRE(FindAttackableHostileOnTile(nearAir, groundedAir.GetTile())
                == &groundedAir);
        REQUIRE(FindAttackableHostileOnTile(
                    nearMissile, groundedMissile.GetTile())
                == &groundedMissile);
        CHECK(CanDeclareAttack(nearAir, groundedAir.GetTile()));
        CHECK(CanDeclareAttack(nearMissile, groundedMissile.GetTile()));
    }

    SECTION("co-located unembarked air over a carrier is not resting")
    {
        // Open sea: air without Air Superiority cannot declare. A friendly deck underfoot
        // does not ground the aircraft for targeting (same as IsRefuelSite).
        rMap.GetTile(8, 10)->SetElevation(-100);
        rMap.GetTile(7, 9)->SetElevation(100);
        Unit& airOverDeck =
            game.MakeUnit(*game.pAi, 8, 10, {"test_flight_chassis", "test_weapon"});
        game.MakeUnit(*game.pAi, 8, 10,
                      {"test_sea_chassis", "test_carrier_deck", "test_weapon"});
        Unit& airAttacker =
            game.MakeUnit(*game.pPlayer, 7, 9, {"test_flight_chassis", "test_weapon"});
        REQUIRE(FindVisibleHostileOnTile(airAttacker, airOverDeck.GetTile())
                == &airOverDeck);
        CHECK(FindAttackableHostileOnTile(airAttacker, airOverDeck.GetTile())
              == nullptr);
        CHECK_FALSE(CanDeclareAttack(airAttacker, airOverDeck.GetTile()));
    }
}