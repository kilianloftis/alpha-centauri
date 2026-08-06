#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/AttackRules.h"
#include "game/units/BaseConquestConfig.h"
#include "game/units/MovementRules.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
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
    GameSettings settings;
    FactionConfig_t playerDefinition;
    FactionConfig_t aiDefinition;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    AttackGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        FillWater_(*pMap);
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, actest::k_TestRngSeed);

        fixtures.dataContext.baseConquestConfig = std::make_unique<BaseConquestConfig_t>();
        fixtures.dataContext.unitComponentRegistry =
            std::make_unique<UnitComponentRegistry>();
        fixtures.dataContext.unitComponentRegistry->Load(FixturePath("unit_components.json"));
        pState->GetUnitOrderExecutor().SetGameDataContext(fixtures.dataContext);

        playerDefinition = fixtures.factionDefinition;
        playerDefinition.id = "player";
        aiDefinition = fixtures.factionDefinition;
        aiDefinition.id = "ai";

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, playerDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, aiDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, actest::k_TestFactionSeed);
        pPlayer = &pState->AddFaction(std::move(pFactionA));
        pAi = &pState->AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y, int pop = 1)
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

        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), fixtures.designs.back(),
            pState->GetWorldMap().GetUnitPositions(), *pTile, pHomeBase);
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
    WorldMap& rMap = game.pState->GetWorldMap();
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
    WorldMap& rMap = game.pState->GetWorldMap();

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
    // Target at (5,4); attacker at (4,4) so they are adjacent.
    const Tile& rTarget = PlaceTarget_(game, rCase.target, 5, 4);
    Unit& rAttacker = PlaceAttacker_(game, rCase.from, bPods, 4, 4);
    const bool bExpected = bPods ? rCase.bAllowedWithPods : rCase.bAllowedWithoutPods;
    CHECK(CanAttackTile(rAttacker, rTarget, game.pState->GetWorldMap()) == bExpected);
}

} // namespace

TEST_CASE("Land attack matrix: reachability plus channel-crossing Permission(Attack)",
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

TEST_CASE("CanAttackTile implies CanEnterTile; land channel still needs pods",
          "[unit][attack][coherence]")
{
    AttackGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    rMap.GetTile(4, 4)->SetElevation(100);
    Unit& land = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    Unit& pods = game.MakeUnit(*game.pPlayer, 3, 4,
                               {"test_chassis", "test_weapon", "test_amphibious"});

    BaseManager& rEnemy = game.MakeBase(*game.pAi, 5, 4);
    CHECK_FALSE(CanEnterTile(land, rEnemy.GetTile(), rMap));
    CHECK_FALSE(CanAttackTile(land, rEnemy.GetTile(), rMap));
    CHECK(CanEnterTile(pods, rEnemy.GetTile(), rMap));
    CHECK(CanAttackTile(pods, rEnemy.GetTile(), rMap));

    rMap.GetTile(3, 4)->SetElevation(100);
    CHECK(CanEnterTile(land, *rMap.GetTile(3, 4), rMap));
    CHECK(CanAttackTile(land, *rMap.GetTile(3, 4), rMap));
}

TEST_CASE("All domains: CanAttackTile requires CanEnterTile", "[unit][attack]")
{
    AttackGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    rMap.GetTile(4, 4)->SetElevation(100);
    rMap.GetTile(5, 5)->SetElevation(-100);
    Unit& sea = game.MakeUnit(*game.pPlayer, 5, 5, {"test_sea_chassis", "test_weapon"});
    Unit& air = game.MakeUnit(*game.pPlayer, 4, 4, {"test_flight_chassis", "test_weapon"});

    CHECK_FALSE(CanAttackTile(sea, *rMap.GetTile(4, 4), rMap)); // shore
    CHECK(CanAttackTile(sea, *rMap.GetTile(6, 5), rMap));        // open water
    // Air can enter land and water, so both attacks are legal.
    CHECK(CanAttackTile(air, *rMap.GetTile(4, 4), rMap));
    CHECK(CanAttackTile(air, *rMap.GetTile(5, 5), rMap));
    CHECK(CanEnterTile(air, *rMap.GetTile(4, 4), rMap));
    CHECK(CanEnterTile(air, *rMap.GetTile(5, 5), rMap));
}

TEST_CASE("FindAttackableHostileOnTile matches TryAttack declare gates", "[unit][attack]")
{
    AttackGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    auto& rEffects = game.pState->GetTileEffects();
    rMap.GetTile(4, 4)->SetElevation(100);
    rMap.GetTile(5, 4)->SetElevation(100);
    Unit& attacker = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = game.MakeUnit(*game.pAi, 5, 4, {"test_chassis", "test_weapon"});

    Unit* pTarget = FindAttackableHostileOnTile(attacker, defender.GetTile(), rMap, rEffects);
    REQUIRE(pTarget == &defender);
    CHECK(CanDeclareAttack(attacker, defender.GetTile(), rMap, rEffects));

    attacker.SetMoveFragmentsRemaining(0);
    CHECK_FALSE(CanDeclareAttack(attacker, defender.GetTile(), rMap, rEffects));
    CHECK(FindVisibleHostileOnTile(attacker, defender.GetTile(), rMap, rEffects) == &defender);
}

TEST_CASE("Embarked cargo defends only in a base; carrier preferred",
          "[unit][attack][defense][transport]")
{
    AttackGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    SECTION("open-sea transport: cargo not selectable")
    {
        rMap.GetTile(5, 5)->SetElevation(-100);
        rMap.GetTile(4, 5)->SetElevation(100);
        Unit& transport = game.MakeUnit(*game.pAi, 5, 5, {"test_sea_chassis", "test_transport"});
        Unit& cargo = game.MakeUnit(*game.pAi, 5, 5, {"test_chassis"});
        REQUIRE(TryAttachToTransport(cargo, rMap));
        Unit& attacker = game.MakeUnit(*game.pPlayer, 4, 5,
                                       {"test_chassis", "test_weapon", "test_amphibious"});

        auto& rEffects = game.pState->GetTileEffects();
        Unit* pTarget =
            FindVisibleHostileOnTile(attacker, *rMap.GetTile(5, 5), rMap, rEffects);
        REQUIRE(pTarget == &transport);
        CHECK(pTarget != &cargo);
        // Shore land without enterability onto open water cannot declare the attack.
        CHECK_FALSE(CanDeclareAttack(attacker, *rMap.GetTile(5, 5), rMap, rEffects));
    }

    SECTION("base: embarked cargo eligible; carrier preferred")
    {
        BaseManager& rBase = game.MakeBase(*game.pAi, 5, 4);
        Unit& transport =
            game.MakeUnit(*game.pAi, 5, 4, {"test_sea_chassis", "test_transport"}, &rBase);
        Unit& cargo = game.MakeUnit(*game.pAi, 5, 4, {"test_chassis"}, &rBase);
        REQUIRE(TryAttachToTransport(cargo, rMap));
        rMap.GetTile(4, 4)->SetElevation(100);
        Unit& attacker = game.MakeUnit(*game.pPlayer, 4, 4,
                                       {"test_chassis", "test_weapon", "test_amphibious"});
        auto& rEffects = game.pState->GetTileEffects();

        Unit* pTarget = FindVisibleHostileOnTile(attacker, rBase.GetTile(), rMap, rEffects);
        REQUIRE(pTarget == &transport);

        game.pAi->GetUnitManager().DestroyUnit(transport);
        Unit* pAfter = FindVisibleHostileOnTile(attacker, rBase.GetTile(), rMap, rEffects);
        REQUIRE(pAfter != nullptr);
        CHECK(pAfter->GetFaction().GetFactionId() == game.pAi->GetFactionId());
    }
}
