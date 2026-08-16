#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/orbital/OrbitalAttack.h"
#include "game/orbital/OrbitalCensus.h"
#include "game/units/InterceptRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementConstants.h"
#include "game/units/MovementRules.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <ranges>

using namespace ac;
using namespace actest;
using json = nlohmann::json;

namespace
{

constexpr int k_point = MovementConstants_t::k_moveFragmentsPerPoint;

struct OrbitalGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    OrbitalGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, actest::k_TestRngSeed);

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed);
        pPlayer = &pState->AddFaction(std::move(pFactionA));
        pAi = &pState->AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            pState->AllocateBaseId(), "TestBase", pTile, fixtures.dataContext,
            pState->GetTileEffects(), pState->GetSecretProjectAvailability());
        REQUIRE(pBase);
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

TEST_CASE("ParseUnitDomain accepts orbital", "[effects][parser][orbital]")
{
    CHECK(EffectConfigParser::ParseUnitDomain("orbital") == UnitDomain_t::Orbital);
    CHECK_THROWS(EffectConfigParser::ParseUnitDomain("space"));
}

TEST_CASE("Orbital domain can enter any terrain", "[movement][orbital]")
{
    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
    Faction& faction = fixture.MakeFaction();
    Unit& missile = fixture.MakeUnit(faction, 4, 4, {"test_orbital_chassis", "test_weapon"});
    CHECK(missile.GetDomain() == UnitDomain_t::Orbital);
    CHECK(CanEnterTileTerrain(missile, fixture.At(4, 4)));
    fixture.At(5, 5).SetElevation(-100);
    CHECK(CanEnterTileTerrain(missile, fixture.At(5, 5)));
}

TEST_CASE("Orbital census counts stackable orbitals for every faction", "[orbital][census]")
{
    OrbitalGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 2, 2);
    BaseManager& aiBase = game.MakeBase(*game.pAi, 6, 6);

    playerBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");
    playerBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");
    aiBase.GetBuildingManager().AddBuilding("Orbital_Power_Transmitter");
    // Non-orbital must not appear.
    playerBase.GetBuildingManager().AddBuilding("granted_hall");

    CHECK(game.pState->CountOrbitalBuildings(game.pPlayer->GetFactionId(), "Sky_Hydroponics_Lab")
          == 2);
    CHECK(game.pState->CountOrbitalBuildings(game.pAi->GetFactionId(), "Orbital_Power_Transmitter")
          == 1);
    CHECK(game.pState->CountOrbitalBuildings(game.pPlayer->GetFactionId(), "granted_hall") == 0);

    const auto census = game.pState->GetOrbitalCensus();
    int sky = 0;
    int power = 0;
    for (const OrbitalCensusEntry_t& rEntry : census)
    {
        if (rEntry.buildingId == "Sky_Hydroponics_Lab")
        {
            sky += rEntry.count;
        }
        if (rEntry.buildingId == "Orbital_Power_Transmitter")
        {
            power += rEntry.count;
        }
        CHECK(rEntry.buildingId != "granted_hall");
    }
    CHECK(sky == 2);
    CHECK(power == 1);
}

TEST_CASE("Sky Hydroponics stacks nutrients on owner bases", "[orbital][effects]")
{
    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 6, 6);

    const int beforeA = baseA.GetNutrientProduction();
    const int beforeB = baseB.GetNutrientProduction();
    baseA.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");
    baseA.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");

    CHECK(baseA.GetNutrientProduction() == beforeA + 2);
    CHECK(baseB.GetNutrientProduction() == beforeB + 2);
}

TEST_CASE("TryAttackSatellite hit destroys one orbital and deploys the pod", "[orbital][asat]")
{
    OrbitalGame_ game;
    BaseManager& attackerBase = game.MakeBase(*game.pPlayer, 2, 2);
    BaseManager& defenderBase = game.MakeBase(*game.pAi, 6, 6);
    attackerBase.GetBuildingManager().AddBuilding("test_odp_always_hit");
    defenderBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");
    defenderBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");

    const int year = game.pState->GetMissionYear();
    REQUIRE(game.pPlayer->CountReadyBuildings("test_odp_always_hit", year) == 1);

    const OrbitalAttackResult_t result = game.pState->TryAttackSatellite(
        *game.pPlayer, *game.pAi, "test_odp_always_hit", "Sky_Hydroponics_Lab");
    CHECK(result.bAttempted);
    CHECK(result.bHit);
    CHECK(result.attackerBuildingId == "test_odp_always_hit");
    CHECK(game.pState->CountOrbitalBuildings(game.pAi->GetFactionId(), "Sky_Hydroponics_Lab") == 1);
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_always_hit", year) == 0);
    // Still unavailable early next year.
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_always_hit", year + 1) == 0);
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_always_hit", year + 2) == 1);
}

TEST_CASE("TryAttackSatellite miss still deploys the pod", "[orbital][asat]")
{
    OrbitalGame_ game;
    BaseManager& attackerBase = game.MakeBase(*game.pPlayer, 2, 2);
    BaseManager& defenderBase = game.MakeBase(*game.pAi, 6, 6);
    attackerBase.GetBuildingManager().AddBuilding("test_odp_always_miss");
    defenderBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");

    const OrbitalAttackResult_t result = game.pState->TryAttackSatellite(
        *game.pPlayer, *game.pAi, "test_odp_always_miss", "Sky_Hydroponics_Lab");
    CHECK(result.bAttempted);
    CHECK_FALSE(result.bHit);
    CHECK_FALSE(result.bAttackerDestroyed);
    CHECK(game.pState->CountOrbitalBuildings(game.pAi->GetFactionId(), "Sky_Hydroponics_Lab") == 1);
    CHECK(game.pPlayer->CountBuildings("test_odp_always_miss") == 1);
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_always_miss", game.pState->GetMissionYear())
          == 0);
}

TEST_CASE("TryAttackSatellite miss can destroy the attacking satellite", "[orbital][asat]")
{
    OrbitalGame_ game;
    BaseManager& attackerBase = game.MakeBase(*game.pPlayer, 2, 2);
    BaseManager& defenderBase = game.MakeBase(*game.pAi, 6, 6);
    attackerBase.GetBuildingManager().AddBuilding("test_odp_miss_destroy");
    defenderBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");

    const OrbitalAttackResult_t result = game.pState->TryAttackSatellite(
        *game.pPlayer, *game.pAi, "test_odp_miss_destroy", "Sky_Hydroponics_Lab");
    CHECK(result.bAttempted);
    CHECK_FALSE(result.bHit);
    CHECK(result.bAttackerDestroyed);
    CHECK(game.pState->CountOrbitalBuildings(game.pAi->GetFactionId(), "Sky_Hydroponics_Lab") == 1);
    CHECK(game.pPlayer->CountBuildings("test_odp_miss_destroy") == 0);
}

TEST_CASE("TryAttackSatellite fails without ready pods or non-orbital target", "[orbital][asat]")
{
    OrbitalGame_ game;
    BaseManager& attackerBase = game.MakeBase(*game.pPlayer, 2, 2);
    BaseManager& defenderBase = game.MakeBase(*game.pAi, 6, 6);
    defenderBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");
    defenderBase.GetBuildingManager().AddBuilding("granted_hall");

    OrbitalAttackResult_t noPod = game.pState->TryAttackSatellite(
        *game.pPlayer, *game.pAi, "test_odp_always_hit", "Sky_Hydroponics_Lab");
    CHECK_FALSE(noPod.bAttempted);

    attackerBase.GetBuildingManager().AddBuilding("test_odp_always_hit");
    OrbitalAttackResult_t nonOrbital = game.pState->TryAttackSatellite(
        *game.pPlayer, *game.pAi, "test_odp_always_hit", "granted_hall");
    CHECK_FALSE(nonOrbital.bAttempted);

    OrbitalAttackResult_t badAttacker = game.pState->TryAttackSatellite(
        *game.pPlayer, *game.pAi, "Sky_Hydroponics_Lab", "Sky_Hydroponics_Lab");
    CHECK_FALSE(badAttacker.bAttempted);
}

TEST_CASE("Deployed pod cannot ASAT then intercept until ready year", "[orbital][deploy]")
{
    OrbitalGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 4, 4);
    BaseManager& aiBase = game.MakeBase(*game.pAi, 6, 6);
    playerBase.GetBuildingManager().AddBuilding("test_odp_always_hit");
    aiBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");

    REQUIRE(game.pState
                ->TryAttackSatellite(*game.pPlayer, *game.pAi, "test_odp_always_hit",
                                     "Sky_Hydroponics_Lab")
                .bAttempted);

    Unit& missile = game.MakeUnit(*game.pAi, 5, 4, {"test_orbital_chassis", "test_weapon"});
    Unit& garrison = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_armor"}, &playerBase);
    missile.SetMoveFragmentsRemaining(missile.GetMovementPoints() * k_point);
    garrison.SetMoveFragmentsRemaining(garrison.GetMovementPoints() * k_point);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        missile, garrison.GetTile());
    // Deployed ODP cannot intercept; combat proceeds (missile may die in combat or survive).
    REQUIRE(result);
    // If intercept had fired, rounds would be empty and attacker destroyed without combat.
    // With ODP deployed, either combat rounds exist or the attack failed for other reasons.
    // Missile is still adjacent and attack was legal — expect combat rounds (not intercept).
    CHECK_FALSE(result->rounds.empty());
}

TEST_CASE("ODP intercepts orbital attacker on a base at 100% chance", "[orbital][intercept]")
{
    OrbitalGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 4, 4);
    playerBase.GetBuildingManager().AddBuilding("test_odp_always_hit");

    Unit& missile = game.MakeUnit(*game.pAi, 5, 4, {"test_orbital_chassis", "test_weapon"});
    Unit& garrison = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_armor"}, &playerBase);
    missile.SetMoveFragmentsRemaining(missile.GetMovementPoints() * k_point);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        missile, garrison.GetTile());
    REQUIRE(result);
    CHECK(result->bAttackerDestroyed);
    CHECK(result->rounds.empty());
    CHECK(std::ranges::distance(game.pAi->GetUnitManager().Units()) == 0);
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_always_hit", game.pState->GetMissionYear())
          == 0);
}

TEST_CASE("ODP intercept miss still deploys and allows combat", "[orbital][intercept]")
{
    OrbitalGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 4, 4);
    playerBase.GetBuildingManager().AddBuilding("test_odp_always_miss");

    Unit& missile = game.MakeUnit(*game.pAi, 5, 4, {"test_orbital_chassis", "test_weapon"});
    Unit& garrison = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_armor"}, &playerBase);
    missile.SetMoveFragmentsRemaining(missile.GetMovementPoints() * k_point);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        missile, garrison.GetTile());
    REQUIRE(result);
    CHECK_FALSE(result->rounds.empty());
    CHECK(game.pPlayer->CountBuildings("test_odp_always_miss") == 1);
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_always_miss", game.pState->GetMissionYear())
          == 0);
}

TEST_CASE("ODP intercept miss can destroy the intercepting satellite", "[orbital][intercept]")
{
    OrbitalGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 4, 4);
    playerBase.GetBuildingManager().AddBuilding("test_odp_miss_destroy");

    Unit& missile = game.MakeUnit(*game.pAi, 5, 4, {"test_orbital_chassis", "test_weapon"});
    Unit& garrison = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_armor"}, &playerBase);
    missile.SetMoveFragmentsRemaining(missile.GetMovementPoints() * k_point);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        missile, garrison.GetTile());
    REQUIRE(result);
    CHECK_FALSE(result->rounds.empty());
    CHECK(game.pPlayer->CountBuildings("test_odp_miss_destroy") == 0);
}

TEST_CASE("A failed intercept destroys the firing base's copy, not another base's",
          "[orbital][intercept]")
{
    // InterceptCandidate_t used to carry only sourceId, so destroy-on-fail re-derived the base
    // with FindBaseWithBuilding — the *first* base owning that id. With the same building in
    // two bases the wrong copy died. The deploy record has to key on the same base, or the
    // survivor stays suppressed for the whole cooldown by a record nothing can erase.
    OrbitalGame_ game;
    // firstBase is created first, so FindBaseWithBuilding would return it.
    BaseManager& firstBase = game.MakeBase(*game.pPlayer, 1, 1);
    BaseManager& defendingBase = game.MakeBase(*game.pPlayer, 4, 4);
    // ThisBase scope: the charge belongs to one base, which is the case pBaseSource exists for.
    firstBase.GetBuildingManager().AddBuilding("test_odp_thisbase_miss_destroy");
    defendingBase.GetBuildingManager().AddBuilding("test_odp_thisbase_miss_destroy");
    REQUIRE(game.pPlayer->CountBuildings("test_odp_thisbase_miss_destroy") == 2);

    Unit& missile = game.MakeUnit(*game.pAi, 5, 4, {"test_orbital_chassis", "test_weapon"});
    Unit& garrison =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_armor"}, &defendingBase);
    missile.SetMoveFragmentsRemaining(missile.GetMovementPoints() * k_point);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(missile, garrison.GetTile());
    REQUIRE(result);

    // The base that actually fired lost its copy; the uninvolved base kept its own.
    // firstBase is the faction's first founded base, so it also holds Headquarters.
    CHECK(defendingBase.GetBuildingManager().GetBuildings().empty());
    CHECK(firstBase.GetBuildingManager().HasBuilding("Headquarters"));
    CHECK(firstBase.GetBuildingManager().HasBuilding("test_odp_thisbase_miss_destroy"));
    CHECK(firstBase.GetBuildingManager().GetBuildings().size() == 2);
    CHECK(game.pPlayer->CountBuildings("test_odp_thisbase_miss_destroy") == 1);

    // And the survivor is usable: its deploy record was erased with the copy that spent it.
    // With deploy and destroy keyed on different bases, this reads 0.
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_thisbase_miss_destroy",
                                            game.pState->GetMissionYear())
          == 1);
}

TEST_CASE("Second ready ODP can still act after the first deploys", "[orbital][deploy]")
{
    OrbitalGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 4, 4);
    BaseManager& aiBase = game.MakeBase(*game.pAi, 6, 6);
    playerBase.GetBuildingManager().AddBuilding("test_odp_always_hit");
    playerBase.GetBuildingManager().AddBuilding("test_odp_always_hit");
    aiBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");
    aiBase.GetBuildingManager().AddBuilding("Orbital_Power_Transmitter");

    REQUIRE(game.pState
                ->TryAttackSatellite(*game.pPlayer, *game.pAi, "test_odp_always_hit",
                                     "Sky_Hydroponics_Lab")
                .bHit);
    const int year = game.pState->GetMissionYear();
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_always_hit", year) == 1);

    REQUIRE(game.pState
                ->TryAttackSatellite(*game.pPlayer, *game.pAi, "test_odp_always_hit",
                                     "Orbital_Power_Transmitter")
                .bHit);
    CHECK(game.pPlayer->CountReadyBuildings("test_odp_always_hit", year) == 0);
}

TEST_CASE("ListReadyOrbitalAttackers lists ready ASAT buildings for selection", "[orbital][asat]")
{
    OrbitalGame_ game;
    BaseManager& attackerBase = game.MakeBase(*game.pPlayer, 2, 2);
    BaseManager& defenderBase = game.MakeBase(*game.pAi, 6, 6);
    defenderBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");
    CHECK(game.pState->ListReadyOrbitalAttackers(*game.pPlayer).empty());

    attackerBase.GetBuildingManager().AddBuilding("test_odp_always_hit");
    attackerBase.GetBuildingManager().AddBuilding("test_odp_always_hit");
    attackerBase.GetBuildingManager().AddBuilding("test_odp_always_miss");
    attackerBase.GetBuildingManager().AddBuilding("Sky_Hydroponics_Lab");

    const auto options = game.pState->ListReadyOrbitalAttackers(*game.pPlayer);
    REQUIRE(options.size() == 2);

    const OrbitalAttackerOption_t* pHit = nullptr;
    const OrbitalAttackerOption_t* pMiss = nullptr;
    for (const OrbitalAttackerOption_t& rOption : options)
    {
        if (rOption.buildingId == "test_odp_always_hit")
        {
            pHit = &rOption;
        }
        if (rOption.buildingId == "test_odp_always_miss")
        {
            pMiss = &rOption;
        }
        CHECK(rOption.buildingId != "Sky_Hydroponics_Lab");
    }
    REQUIRE(pHit);
    REQUIRE(pMiss);
    CHECK(pHit->readyCount == 2);
    CHECK(pHit->chance == 100);
    CHECK(pHit->pConfig != nullptr);
    CHECK(pMiss->readyCount == 1);
    CHECK(pMiss->chance == 0);

    REQUIRE(game.pState
                ->TryAttackSatellite(*game.pPlayer, *game.pAi, "test_odp_always_miss",
                                     "Sky_Hydroponics_Lab")
                .bAttempted);

    const auto after = game.pState->ListReadyOrbitalAttackers(*game.pPlayer);
    REQUIRE(after.size() == 1);
    CHECK(after.front().buildingId == "test_odp_always_hit");
    CHECK(after.front().readyCount == 2);
}

TEST_CASE("ThisUnit InterceptAttempt fires for the defending unit only", "[orbital][intercept]")
{
    OrbitalGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 4, 4);

    Unit& missile = game.MakeUnit(*game.pAi, 5, 4, {"test_orbital_chassis", "test_weapon"});
    Unit& samGarrison =
        game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_armor", "test_sam_escort"},
                      &playerBase);
    missile.SetMoveFragmentsRemaining(missile.GetMovementPoints() * k_point);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        missile, samGarrison.GetTile());
    REQUIRE(result);
    CHECK(result->bAttackerDestroyed);
    CHECK(result->rounds.empty());
}

TEST_CASE("ThisTile InterceptAttempt fires on the battery tile", "[orbital][intercept]")
{
    OrbitalGame_ game;
    BaseManager& playerBase = game.MakeBase(*game.pPlayer, 4, 4);
    game.pState->GetTileEffects().AddImprovementWithEffects(
        *game.pState->GetWorldMap().GetTile(4, 4), "test_sam_battery");

    Unit& missile = game.MakeUnit(*game.pAi, 5, 4, {"test_orbital_chassis", "test_weapon"});
    Unit& garrison = game.MakeUnit(*game.pPlayer, 4, 4, {"test_chassis", "test_armor"}, &playerBase);
    missile.SetMoveFragmentsRemaining(missile.GetMovementPoints() * k_point);

    auto result = game.pState->GetUnitOrderExecutor().TryAttack(
        missile, garrison.GetTile());
    REQUIRE(result);
    CHECK(result->bAttackerDestroyed);
    CHECK(result->rounds.empty());
}

TEST_CASE("Parse OrbitalAttack and InterceptAttempt effects", "[effects][parser][orbital]")
{
    const EffectConfig_t attack = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "OrbitalAttack",
        "scope": "FactionGlobal",
        "parameters": {
            "chance": 50,
            "cooldown_turns": 1,
            "chance_of_destruction_on_fail": 50
        }
    })"));
    const auto* pAttack = std::get_if<OrbitalAttackEffect_t>(&attack.effect);
    REQUIRE(pAttack);
    CHECK(pAttack->chance == 50);
    CHECK(pAttack->cooldownTurns == 1);
    CHECK(pAttack->chanceOfDestructionOnFail == 50);

    const EffectConfig_t intercept = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InterceptAttempt",
        "scope": "FactionGlobal",
        "parameters": {
            "chance": 50,
            "cooldown_turns": 1,
            "chance_of_destruction_on_fail": 0
        },
        "unitFilter": { "kind": "Domain", "domain": "orbital" },
        "condition": { "kind": "TargetTileHas", "value": "Base" }
    })"));
    const auto* pIntercept = std::get_if<InterceptAttemptEffect_t>(&intercept.effect);
    REQUIRE(pIntercept);
    CHECK(pIntercept->chance == 50);
    CHECK(pIntercept->chanceOfDestructionOnFail == 0);
    CHECK(intercept.unitFilter.has_value());
    CHECK(intercept.condition.has_value());

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InterceptAttempt",
        "scope": "FactionGlobal",
        "parameters": { "chance": 50 }
    })")));

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "OrbitalAttack",
        "scope": "FactionGlobal",
        "parameters": { "chance": 50 }
    })")));
}
