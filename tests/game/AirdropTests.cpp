#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/AirdropRules.h"
#include "game/units/CombatResolver.h"
#include "game/units/MovementConstants.h"
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

void FillLand_(WorldMap& rMap)
{
    for (auto& pTile : rMap.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

struct AirdropGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    FactionConfig_t playerDefinition;
    FactionConfig_t aiDefinition;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    AirdropGame_()
    {
        auto pMap = std::make_unique<WorldMap>(20, 20);
        FillLand_(*pMap);
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
            fixtures.dataContext.interactionGrids, actest::k_TestRngSeed);

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

    void LatchTurnStart(Unit& rUnit)
    {
        rUnit.BeginTurn();
    }
};

} // namespace

TEST_CASE("Airdrop requires Drop Pods, launch pad, and full moves", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    auto& rEffects = game.pState->GetTileEffects();

    BaseManager& rBase = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& withoutPods = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon"}, &rBase);
    game.LatchTurnStart(withoutPods);
    CHECK(CanAttemptAirdrop(withoutPods).failReason == AirdropFailReason_t::NotCapable);

    Unit& withPods =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "drop_pods"}, &rBase);
    game.LatchTurnStart(withPods);
    CHECK(CanAttemptAirdrop(withPods).Ok());

    withPods.SetMoveFragmentsRemaining(withPods.GetMoveFragmentsRemaining() - 1);
    CHECK(CanAttemptAirdrop(withPods).failReason == AirdropFailReason_t::NoMovesRemaining);
    game.LatchTurnStart(withPods);

    Tile* pOffPad = rMap.GetTile(5, 4);
    REQUIRE(pOffPad);
    rMap.GetUnitPositions().MoveUnit(withPods, *pOffPad);
    CHECK(CanAttemptAirdrop(withPods).failReason == AirdropFailReason_t::NotOnLaunchPad);

    Tile* pNear = rMap.GetTile(6, 4);
    REQUIRE(pNear);
    CHECK(CanAirdropTo(withPods, *pNear, rMap, rEffects).failReason
          == AirdropFailReason_t::NotOnLaunchPad);
}

TEST_CASE("Airdrop cannot chain through a friendly launch pad", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    BaseManager& rHome = game.MakeBase(*game.pPlayer, 4, 4);
    BaseManager& rOtherPad = game.MakeBase(*game.pPlayer, 8, 4);
    Unit& dropper =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "drop_pods"}, &rHome);
    game.LatchTurnStart(dropper);

    auto result = game.pState->GetUnitOrderExecutor().TryAirdrop(dropper, rOtherPad.GetTile());
    REQUIRE(result.Ok());
    CHECK(dropper.HasAirdroppedThisTurn());
    CHECK(CanAttemptAirdrop(dropper).failReason == AirdropFailReason_t::AlreadyAirdropped);

    Tile* pField = rMap.GetTile(6, 4);
    REQUIRE(pField);
    result = game.pState->GetUnitOrderExecutor().TryAirdrop(dropper, *pField);
    CHECK(result.failReason == AirdropFailReason_t::AlreadyAirdropped);
}

TEST_CASE("Airdrop range 8 vs orbital insertion", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    auto& rEffects = game.pState->GetTileEffects();

    BaseManager& rBase = game.MakeBase(*game.pPlayer, 2, 2);
    Unit& dropper =
        game.MakeUnit(*game.pPlayer, 2, 2, {"test_chassis", "test_weapon", "drop_pods"}, &rBase);
    game.LatchTurnStart(dropper);

    Tile* pInRange = rMap.GetTile(2 + 8, 2);
    Tile* pOutOfRange = rMap.GetTile(2 + 9, 2);
    REQUIRE(pInRange);
    REQUIRE(pOutOfRange);
    CHECK(CanAirdropTo(dropper, *pInRange, rMap, rEffects).Ok());
    CHECK(CanAirdropTo(dropper, *pOutOfRange, rMap, rEffects).failReason
          == AirdropFailReason_t::OutOfRange);

    game.pPlayer->GetResearch().AddDiscoveredTech("graviton_theory");
    CHECK(ResolveFlag(*game.pPlayer, RuleFlagId_t::OrbitalInsertion));
    CHECK(CanAirdropTo(dropper, *pOutOfRange, rMap, rEffects).Ok());
}

TEST_CASE("Airdrop landing damage and airdrop_launch skip", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    BaseManager& rHome = game.MakeBase(*game.pPlayer, 4, 4);
    BaseManager& rOtherPad = game.MakeBase(*game.pPlayer, 8, 4);
    Unit& dropper =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "drop_pods"}, &rHome);
    game.LatchTurnStart(dropper);
    const int maxHp = dropper.GetCurrentHp();
    REQUIRE(maxHp == 10);

    Tile* pField = rMap.GetTile(6, 4);
    REQUIRE(pField);
    auto result = game.pState->GetUnitOrderExecutor().TryAirdrop(dropper, *pField);
    REQUIRE(result.Ok());
    // 20% of 10 HP = 2
    CHECK(dropper.GetCurrentHp() == maxHp - 2);
    CHECK(dropper.HasAirdroppedThisTurn());
    CHECK(&dropper.GetTile() == pField);

    Unit& toPad =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "drop_pods"}, &rHome);
    game.LatchTurnStart(toPad);
    const int padHp = toPad.GetCurrentHp();
    result = game.pState->GetUnitOrderExecutor().TryAirdrop(toPad, rOtherPad.GetTile());
    REQUIRE(result.Ok());
    CHECK(toPad.GetCurrentHp() == padHp);
}

TEST_CASE("Singularity increases airdrop landing damage", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    BaseManager& rHome = game.MakeBase(*game.pPlayer, 4, 4);
    // Chassis 10 + singularity 40 = 50 HP; damage 20+6 = 26% → 13
    Unit& dropper = game.MakeUnit(
        *game.pPlayer, 4, 4,
        {"test_chassis", "test_weapon", "drop_pods", "singularity_inductor"}, &rHome);
    game.LatchTurnStart(dropper);
    REQUIRE(dropper.GetCurrentHp() == 50);
    CHECK(ResolveStat(dropper, StatId_t::AirdropLandingDamage) == 26);

    Tile* pField = rMap.GetTile(6, 4);
    REQUIRE(pField);
    REQUIRE(game.pState->GetUnitOrderExecutor().TryAirdrop(dropper, *pField).Ok());
    CHECK(dropper.GetCurrentHp() == 50 - 13);
}

TEST_CASE("Airdrop rejects enemy-occupied tiles and interdiction", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    auto& rEffects = game.pState->GetTileEffects();

    BaseManager& rHome = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& dropper =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "drop_pods"}, &rHome);
    game.LatchTurnStart(dropper);

    Tile* pDest = rMap.GetTile(6, 4);
    REQUIRE(pDest);
    game.MakeUnit(*game.pAi, 6, 4, {"test_chassis", "test_weapon"});
    CHECK(CanAirdropTo(dropper, *pDest, rMap, rEffects).failReason
          == AirdropFailReason_t::EnemyOccupied);

    // Clear occupant by using another dest; place unmoved air superiority nearby.
    Tile* pClear = rMap.GetTile(7, 4);
    REQUIRE(pClear);
    Unit& interceptor =
        game.MakeUnit(*game.pAi, 7, 6, {"test_flight_chassis", "test_weapon", "air_superiority"});
    interceptor.SetMoveFragmentsRemaining(
        interceptor.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint);
    CHECK(ResolveStat(interceptor, StatId_t::AirdropInterdictionRadius) == 2);
    CHECK(CanAirdropTo(dropper, *pClear, rMap, rEffects).failReason
          == AirdropFailReason_t::Interdicted);

    interceptor.SpendMoveFragments(MovementConstants_t::k_moveFragmentsPerPoint);
    CHECK(CanAirdropTo(dropper, *pClear, rMap, rEffects).Ok());
}

TEST_CASE("Friendly interceptor does not block airdrop", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    auto& rEffects = game.pState->GetTileEffects();

    BaseManager& rHome = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& dropper =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "drop_pods"}, &rHome);
    game.LatchTurnStart(dropper);

    Unit& friendly =
        game.MakeUnit(*game.pPlayer, 7, 6, {"test_flight_chassis", "test_weapon", "air_superiority"});
    friendly.SetMoveFragmentsRemaining(
        friendly.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint);

    Tile* pClear = rMap.GetTile(7, 4);
    REQUIRE(pClear);
    CHECK(CanAirdropTo(dropper, *pClear, rMap, rEffects).Ok());
}

TEST_CASE("Noncombat airdrop spends remaining moves; combat keeps them", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    BaseManager& rHome = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& combat =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "drop_pods"}, &rHome);
    Unit& former =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_terraformer", "drop_pods"}, &rHome);
    game.LatchTurnStart(combat);
    game.LatchTurnStart(former);
    REQUIRE(combat.IsCombatUnit());
    REQUIRE_FALSE(former.IsCombatUnit());

    Tile* pA = rMap.GetTile(5, 4);
    Tile* pB = rMap.GetTile(6, 5);
    REQUIRE(pA);
    REQUIRE(pB);

    const int combatMoves = combat.GetMoveFragmentsRemaining();
    REQUIRE(game.pState->GetUnitOrderExecutor().TryAirdrop(combat, *pA).Ok());
    CHECK(combat.GetMoveFragmentsRemaining() == combatMoves);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryAirdrop(former, *pB).Ok());
    CHECK(former.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("Post-airdrop attack applies HasAirdroppedThisTurn penalty", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    BaseManager& rHome = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& attacker =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_weapon", "drop_pods"}, &rHome);
    game.LatchTurnStart(attacker);

    Tile* pLand = rMap.GetTile(5, 4);
    REQUIRE(pLand);
    REQUIRE(game.pState->GetUnitOrderExecutor().TryAirdrop(attacker, *pLand).Ok());
    REQUIRE(attacker.HasAirdroppedThisTurn());

    Unit& defender = game.MakeUnit(*game.pAi, 6, 4, {"test_chassis", "test_weapon"});
    EffectContext_t ctx;
    ctx.pUnit = &attacker;
    ctx.pAttacker = &attacker;
    ctx.combatRole = CombatRole_t::Attacker;
    const int penalized = ResolveStat(attacker, StatId_t::Attack, ctx);
    attacker.ClearAirdroppedThisTurn();
    const int normal = ResolveStat(attacker, StatId_t::Attack, ctx);
    CHECK(penalized < normal);
    (void)defender;
    (void)rMap;
}

TEST_CASE("Airdrop into empty foreign base captures via arrival", "[unit][airdrop]")
{
    AirdropGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    BaseManager& rHome = game.MakeBase(*game.pPlayer, 2, 2);
    // CapturePopLoss is 1; size must exceed that so the base survives to transfer.
    BaseManager& rEnemyBase = game.MakeBase(*game.pAi, 8, 2, /*pop=*/3);
    REQUIRE(rEnemyBase.GetFaction().GetFactionId() == game.pAi->GetFactionId());

    Unit& dropper =
        game.MakeUnit(*game.pPlayer, 2, 2, {"test_chassis", "test_weapon", "drop_pods"}, &rHome);
    game.LatchTurnStart(dropper);
    game.pPlayer->GetResearch().AddDiscoveredTech("graviton_theory");

    auto result = game.pState->GetUnitOrderExecutor().TryAirdrop(dropper, rEnemyBase.GetTile());
    REQUIRE(result.Ok());
    CHECK(&dropper.GetTile() == &rEnemyBase.GetTile());
    CHECK(rEnemyBase.GetFaction().GetFactionId() == game.pPlayer->GetFactionId());
    (void)rMap;
}
