#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementConstants.h"
#include "game/units/Pathfinder.h"
#include "game/units/ScrambleRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;
using json = nlohmann::json;

namespace
{

void FillLand_(WorldMap& rMap)
{
    for (auto& pTile : rMap.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

struct ScrambleGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    FactionConfig_t playerDefinition;
    FactionConfig_t aiDefinition;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    ScrambleGame_()
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

    Unit& MakeUnit(Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds)
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
            pState->GetWorldMap().GetUnitPositions(), *pTile, nullptr);
    }

    void FullMoves(Unit& rUnit)
    {
        rUnit.SetMoveFragmentsRemaining(
            rUnit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint);
    }
};

} // namespace

TEST_CASE("Parse Scramble requires condition and range", "[effects][parser][scramble]")
{
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Scramble", "scope": "ThisUnit"
    })")));

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Scramble",
        "scope": "ThisUnit",
        "condition": { "kind": "AttackerDomain", "domains": ["air"] }
    })")));

    const EffectConfig_t scramble = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Scramble",
        "scope": "ThisUnit",
        "parameters": { "range": 2 },
        "condition": { "kind": "AttackerDomain", "domains": ["air"] }
    })"));
    const auto* pScramble = std::get_if<ScrambleEffect_t>(&scramble.effect);
    REQUIRE(pScramble);
    CHECK(pScramble->range == 2);
    REQUIRE(scramble.condition);
}

TEST_CASE("Air Superiority scrambler becomes combat defender", "[unit][scramble]")
{
    ScrambleGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    Unit& ground = game.MakeUnit(*game.pAi, 5, 5, {"test_chassis", "test_weapon"});
    Unit& scrambler =
        game.MakeUnit(*game.pAi, 5, 7, {"test_flight_chassis", "test_weapon", "air_superiority"});
    game.FullMoves(scrambler);
    Unit& attacker =
        game.MakeUnit(*game.pPlayer, 4, 5, {"test_flight_chassis", "test_weapon"});
    game.FullMoves(attacker);

    const Tile& rGroundTile = ground.GetTile();
    const UnitId_t scramblerId = scrambler.GetUnitId();
    const UnitId_t groundId = ground.GetUnitId();

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(attacker, rGroundTile);
    REQUIRE(result);
    CHECK(result->defenderId == scramblerId);
    CHECK(result->defenderId != groundId);
    REQUIRE(result->scramblePath.size() == 2);
    CHECK(result->scramblePath.back() == &rGroundTile);

    // Scramble walks the path before Resolve; the unit may then die in combat — only
    // inspect the live scrambler when it survived.
    if (!result->bDefenderDestroyed)
    {
        Unit* pScrambler = nullptr;
        for (Unit& rUnit : game.pAi->GetUnitManager().Units())
        {
            if (rUnit.GetUnitId() == scramblerId)
            {
                pScrambler = &rUnit;
                break;
            }
        }
        REQUIRE(pScrambler);
        CHECK(&pScrambler->GetTile() == &rGroundTile);
        CHECK(pScrambler->GetMoveFragmentsRemaining() == 0);
    }
}

TEST_CASE("Scramble skips insufficient moves, out of radius, wrong faction, and land attackers",
          "[unit][scramble]")
{
    ScrambleGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    auto& rEffects = game.pState->GetTileEffects();
    const Pathfinder& rPathfinder = game.pState->GetPathfinder();

    Unit& ground = game.MakeUnit(*game.pAi, 5, 5, {"test_chassis", "test_weapon"});

    SECTION("insufficient remaining fragments for the path")
    {
        // Distance 2 costs two move points; one spent leaves not enough to arrive.
        Unit& scrambler =
            game.MakeUnit(*game.pAi, 5, 7,
                          {"test_flight_chassis", "test_weapon", "air_superiority"});
        scrambler.SpendMoveFragments(MovementConstants_t::k_moveFragmentsPerPoint);
        Unit& attacker =
            game.MakeUnit(*game.pPlayer, 4, 5, {"test_flight_chassis", "test_weapon"});
        CHECK(FindScrambler(attacker, ground, rMap, rEffects, rPathfinder)
              == nullptr);
    }

    SECTION("out of radius")
    {
        Unit& scrambler =
            game.MakeUnit(*game.pAi, 5, 10,
                          {"test_flight_chassis", "test_weapon", "air_superiority"});
        game.FullMoves(scrambler);
        Unit& attacker =
            game.MakeUnit(*game.pPlayer, 4, 5, {"test_flight_chassis", "test_weapon"});
        CHECK(FindScrambler(attacker, ground, rMap, rEffects, rPathfinder)
              == nullptr);
    }

    SECTION("enemy air superiority does not scramble for the defender")
    {
        Unit& enemyScrambler =
            game.MakeUnit(*game.pPlayer, 5, 7,
                          {"test_flight_chassis", "test_weapon", "air_superiority"});
        game.FullMoves(enemyScrambler);
        Unit& attacker =
            game.MakeUnit(*game.pPlayer, 4, 5, {"test_flight_chassis", "test_weapon"});
        CHECK(FindScrambler(attacker, ground, rMap, rEffects, rPathfinder)
              == nullptr);
    }

    SECTION("land attacker fails Domain air condition")
    {
        Unit& scrambler =
            game.MakeUnit(*game.pAi, 5, 7,
                          {"test_flight_chassis", "test_weapon", "air_superiority"});
        game.FullMoves(scrambler);
        const Tile& rScramblerTile = scrambler.GetTile();
        Unit& landAttacker =
            game.MakeUnit(*game.pPlayer, 4, 5, {"test_chassis", "test_weapon"});
        CHECK(FindScrambler(landAttacker, ground, rMap, rEffects, rPathfinder)
              == nullptr);

        const UnitId_t groundId = ground.GetUnitId();
        auto result =
            game.pState->GetUnitOrderExecutor().TryAttack(landAttacker, ground.GetTile());
        REQUIRE(result);
        CHECK(result->defenderId == groundId);
        CHECK(result->scramblePath.empty());
        CHECK(&scrambler.GetTile() == &rScramblerTile);
    }
}

TEST_CASE("Scramble ranking prefers higher Attack then higher HP", "[unit][scramble]")
{
    ScrambleGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();
    auto& rEffects = game.pState->GetTileEffects();
    const Pathfinder& rPathfinder = game.pState->GetPathfinder();

    Unit& ground = game.MakeUnit(*game.pAi, 5, 5, {"test_chassis", "test_weapon"});
    Unit& attacker =
        game.MakeUnit(*game.pPlayer, 4, 5, {"test_flight_chassis", "test_weapon"});

    SECTION("higher Attack wins")
    {
        Unit& weak =
            game.MakeUnit(*game.pAi, 5, 7,
                          {"test_flight_chassis", "test_weak_weapon", "air_superiority"});
        Unit& strong =
            game.MakeUnit(*game.pAi, 6, 7,
                          {"test_flight_chassis", "test_weapon", "air_superiority"});
        game.FullMoves(weak);
        game.FullMoves(strong);
        CHECK(FindScrambler(attacker, ground, rMap, rEffects, rPathfinder)
              == &strong);
    }

    SECTION("equal Attack prefers higher HP")
    {
        Unit& lowHp =
            game.MakeUnit(*game.pAi, 5, 7,
                          {"test_flight_chassis", "test_weapon", "air_superiority"});
        Unit& highHp =
            game.MakeUnit(*game.pAi, 6, 7,
                          {"test_flight_chassis", "test_weapon", "air_superiority"});
        game.FullMoves(lowHp);
        game.FullMoves(highHp);
        lowHp.SetCurrentHp(3);
        highHp.SetCurrentHp(8);
        CHECK(FindScrambler(attacker, ground, rMap, rEffects, rPathfinder)
              == &highHp);
    }
}

TEST_CASE("Scramble MoveOrder walks hop by hop via Execute", "[unit][scramble]")
{
    ScrambleGame_ game;
    WorldMap& rMap = game.pState->GetWorldMap();

    Unit& ground = game.MakeUnit(*game.pAi, 5, 5, {"test_chassis", "test_weapon"});
    Unit& scrambler =
        game.MakeUnit(*game.pAi, 5, 7, {"test_flight_chassis", "test_weapon", "air_superiority"});
    game.FullMoves(scrambler);
    Unit& attacker =
        game.MakeUnit(*game.pPlayer, 4, 5, {"test_flight_chassis", "test_weapon"});
    game.FullMoves(attacker);

    std::vector<const Tile*> tilesSeen;
    auto movedConn = rMap.GetUnitPositions().OnUnitMoved.ConnectScoped(
        [&](Unit& rUnit)
        {
            if (&rUnit == &scrambler)
            {
                tilesSeen.push_back(&rUnit.GetTile());
            }
        });

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(attacker, ground.GetTile());
    REQUIRE(result);
    REQUIRE(result->scramblePath.size() == 2);
    CHECK(tilesSeen == result->scramblePath);
    CHECK(result->scramblePath.back() == &ground.GetTile());
    CHECK_FALSE(scrambler.GetOrder().has_value());
}
