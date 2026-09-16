#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/CommerceCalculator.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <memory>

using namespace ac;
using namespace actest;

namespace
{

struct CommerceGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pA = nullptr;
    Faction* pB = nullptr;

    CommerceGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
            fixtures.dataContext.interactionGrids, k_TestRngSeed);

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, k_TestFactionSeed);

        pA = &pState->AddFaction(std::move(pFactionA));
        pB = &pState->AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeHqBase(Faction& rFaction, int x, int y)
    {
        BaseManager& rBase = fixtures.MakeFactionBase(rFaction, x, y);
        rBase.GetBuildingManager().AddBuilding("Headquarters");
        rBase.GetBuildingManager().AddBuilding("world_beacon");
        return rBase;
    }
};

int ExpectedPairRaw_(int energyA, int energyB)
{
    return static_cast<int>(std::ceil(static_cast<double>(energyA + energyB) * 0.125));
}

} // namespace

TEST_CASE("ComputeForBase reports our and their energy for a paired partner", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const auto lines = CommerceCalculator{}.ComputeForBase(a1, *game.pState);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].pPartner == game.pB);
    CHECK(lines[0].status == DiplomaticStatus_t::Pact);

    const int expectedOurs =
        ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    const int expectedTheirs =
        ExpectedPairRaw_(b1.GetEnergyProduction(), a1.GetEnergyProduction());
    CHECK(lines[0].ourEnergy == expectedOurs);
    CHECK(lines[0].theirEnergy == expectedTheirs);
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == expectedOurs);
}

TEST_CASE("ComputeForBase is empty for surplus bases and non-commerce treaties", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& a2 = game.MakeHqBase(*game.pA, 2, 6);
    game.MakeHqBase(*game.pB, 6, 2);
    a1.GetBuildingManager().AddBuilding("energy_tap");
    REQUIRE(a1.GetEnergyProduction() > a2.GetEnergyProduction());

    DiplomacyLedger& rDiplomacy = game.pState->GetDiplomacyLedger();
    rDiplomacy.SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);
    CHECK(CommerceCalculator{}.ComputeForBase(a2, *game.pState).empty());

    rDiplomacy.SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Truce);
    CHECK(CommerceCalculator{}.ComputeForBase(a1, *game.pState).empty());
}

TEST_CASE("Commerce pairs by pre-commerce energy; surplus bases ignored", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& a2 = game.MakeHqBase(*game.pA, 2, 6);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    a1.GetBuildingManager().AddBuilding("energy_tap");
    REQUIRE(a1.GetEnergyProduction() > a2.GetEnergyProduction());

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const auto commerce = CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState);
    REQUIRE(commerce.count(a1.GetBaseId()) == 1);
    CHECK(commerce.count(a2.GetBaseId()) == 0);

    const int expected = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    CHECK(commerce.at(a1.GetBaseId()) == expected);
}

TEST_CASE("Friendship applies treaty_multiplier; Pact does not", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    const int pairRaw = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    REQUIRE(pairRaw >= 2);

    DiplomacyLedger& rDiplomacy = game.pState->GetDiplomacyLedger();
    rDiplomacy.SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(),
                         DiplomaticStatus_t::Friendship);
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == static_cast<int>(std::floor(pairRaw * 0.5)));

    rDiplomacy.SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == pairRaw);
}

TEST_CASE("No commerce for Truce, None, or Vendetta", "[commerce]")
{
    CommerceGame_ game;
    game.MakeHqBase(*game.pA, 2, 2);
    game.MakeHqBase(*game.pB, 6, 2);

    DiplomacyLedger& rDiplomacy = game.pState->GetDiplomacyLedger();
    CommerceCalculator calc;

    rDiplomacy.SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Truce);
    CHECK(calc.ComputeForFaction(*game.pA, *game.pState).empty());

    rDiplomacy.SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::None);
    CHECK(calc.ComputeForFaction(*game.pA, *game.pState).empty());

    rDiplomacy.SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Vendetta);
    CHECK(calc.ComputeForFaction(*game.pA, *game.pState).empty());
}

TEST_CASE("CommerceRate doubles pair value when present", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);
    a1.GetBuildingManager().AddBuilding("commerce_rate_doubler");

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const int pairRaw = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == pairRaw * 2);
}

TEST_CASE("commerce_rating and economic techs feed the tech ratio; total ignores rating",
          "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);
    a1.GetBuildingManager().AddBuilding("commerce_rating_shrine"); // +2 CommerceRating

    game.pA->GetResearch().AddDiscoveredTech("industrial_automation"); // +1 economic tech
    game.pB->GetResearch().AddDiscoveredTech("planetary_economics");   // +1 economic tech (total)

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    // commerceTech = Resolve(CommerceRating) = shrine +2 + tech +1 = 3
    // totalCommerceTech = tech-only Adds across factions = 2
    // value = pairRaw * (3+1)/(2+1) = pairRaw * 4/3
    const int pairRaw = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == (pairRaw * 4) / 3);
}

TEST_CASE("CommerceEnergyBonus adds at the end of each pair", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);
    a1.GetBuildingManager().AddBuilding("commerce_energy_bonus_shrine");

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const int pairRaw = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == pairRaw + 1);
}

TEST_CASE("Commerce feeds ResourceManager raw energy before the econ split", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const int commerce =
        CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId());
    REQUIRE(commerce > 0);

    const int econWithout = a1.GetEconProduction(0);
    const int econWith = a1.GetEconProduction(commerce);
    CHECK(econWith > econWithout);

    const int treasuryBefore = game.pA->GetEconomy().GetEnergy();
    game.pA->ProduceBaseResources(*game.pState);
    game.pA->CollectIncome();
    CHECK(game.pA->GetEconomy().GetEnergy() == treasuryBefore + econWith);
    (void)b1;
}
