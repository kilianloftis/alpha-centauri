#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/DiplomaticActionExecutor.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/TradeItem.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace ac;
using namespace actest;

namespace
{

struct DiplomacyGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;
    Faction* pThird = nullptr;

    DiplomacyGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator);

        auto pA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext);
        auto pB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext);
        auto pC = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext);

        pPlayer = &pState->AddFaction(std::move(pA));
        pAi = &pState->AddFaction(std::move(pB));
        pThird = &pState->AddFaction(std::move(pC));

        pState->GetDiplomacyLedger().SetKnown(pPlayer->GetFactionId(), pAi->GetFactionId());
        pState->GetDiplomacyLedger().SetKnown(pPlayer->GetFactionId(), pThird->GetFactionId());
        pState->GetDiplomacyLedger().SetKnown(pAi->GetFactionId(), pThird->GetFactionId());
    }
};

} // namespace

TEST_CASE("Propose truce to AI is accepted and applied", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.requestedStatus = DiplomaticStatus::Truce;

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Accepted);
    CHECK(game.pState->GetDiplomacyLedger().HasTruce(proposal.proposer, proposal.recipient));
}

TEST_CASE("Propose to player stays pending until Accept", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    DiplomaticProposal_t proposal;
    proposal.proposer = game.pAi->GetFactionId();
    proposal.recipient = game.pPlayer->GetFactionId();
    proposal.requestedStatus = DiplomaticStatus::Truce;

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::PendingPlayer);
    CHECK_FALSE(game.pState->GetDiplomacyLedger().HasTruce(proposal.proposer, proposal.recipient));

    REQUIRE(game.pState->GetDiplomaticActionExecutor().Accept(*game.pState));
    CHECK(game.pState->GetDiplomacyLedger().HasTruce(proposal.proposer, proposal.recipient));
}

TEST_CASE("Energy credits trade moves treasury", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    game.pPlayer->GetEconomy().AddEnergy(50);
    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.give.push_back(TradeCredits_t{20});

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Accepted);
    CHECK(game.pPlayer->GetEconomy().GetEnergy() == 30);
    CHECK(game.pAi->GetEconomy().GetEnergy() == 20);
}

TEST_CASE("Technology trade grants tech to recipient", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    game.pPlayer->GetResearch().AddDiscoveredTech("test_tech");
    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.give.push_back(TradeTechnology_t{"test_tech"});

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Accepted);
    CHECK(game.pAi->GetResearch().HasDiscoveredTech("test_tech"));
}

TEST_CASE("Comm frequency introduces third faction", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    // Break known between AI and third so intro is meaningful.
    game.pState->GetDiplomacyLedger().SetKnown(
        game.pAi->GetFactionId(), game.pThird->GetFactionId(), false);

    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.give.push_back(TradeCommFrequency_t{game.pThird->GetFactionId()});

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Accepted);
    CHECK(game.pState->GetDiplomacyLedger().AreKnown(
        game.pAi->GetFactionId(), game.pThird->GetFactionId()));
}

TEST_CASE("Third-party vendetta trade sets status", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    game.pState->GetDiplomacyLedger().SetStatus(
        game.pPlayer->GetFactionId(), game.pAi->GetFactionId(), DiplomaticStatus::Pact);
    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.give.push_back(TradeDeclareVendetta_t{game.pThird->GetFactionId()});

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Accepted);
    CHECK(game.pState->GetDiplomacyLedger().HasVendetta(
        game.pAi->GetFactionId(), game.pThird->GetFactionId()));
}

TEST_CASE("World map trade merges explored tiles", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    Tile* pTile = game.pState->GetWorldMap().GetTile(3, 3);
    REQUIRE(pTile);
    game.pPlayer->GetExploredMap().Mark(*pTile);
    REQUIRE(game.pPlayer->GetExploredMap().IsExplored(3, 3));
    CHECK_FALSE(game.pAi->GetExploredMap().IsExplored(3, 3));

    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.give.push_back(TradeWorldMap_t{});

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Accepted);
    CHECK(game.pAi->GetExploredMap().IsExplored(3, 3));
}

TEST_CASE("Trade under vendetta is invalid", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    game.pState->GetDiplomacyLedger().SetStatus(
        game.pPlayer->GetFactionId(), game.pAi->GetFactionId(), DiplomaticStatus::Vendetta);
    game.pPlayer->GetEconomy().AddEnergy(10);

    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.give.push_back(TradeCredits_t{5});

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Invalid);
}

TEST_CASE("Base transfer without Pact is invalid", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    BaseManager* pBase = game.pPlayer->CreateBase(
        game.pState->AllocateBaseId(), "Gift",
        game.pState->GetWorldMap().GetTile(2, 2),
        game.fixtures.dataContext,
        game.pState->GetTileEffects(),
        game.pState->GetSecretProjectAvailability());
    REQUIRE(pBase);

    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.give.push_back(TradeBase_t{pBase->GetBaseId()});

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Invalid);
}

TEST_CASE("Base transfer changes ownership", "[diplomacy][executor]")
{
    DiplomacyGame_ game;
    game.pState->GetDiplomacyLedger().SetStatus(
        game.pPlayer->GetFactionId(), game.pAi->GetFactionId(), DiplomaticStatus::Pact);
    BaseManager* pBase = game.pPlayer->CreateBase(
        game.pState->AllocateBaseId(), "Gift",
        game.pState->GetWorldMap().GetTile(2, 2),
        game.fixtures.dataContext,
        game.pState->GetTileEffects(),
        game.pState->GetSecretProjectAvailability());
    REQUIRE(pBase);
    const BaseId_t baseId = pBase->GetBaseId();
    const int popSize = pBase->GetPopulation().GetSize();
    pBase->GetBuildingManager().AddBuilding("flat_nutrient");
    pBase->GetPopulation().SetNutrientStockpile(17);
    pBase->GetProduction().SetProduction(
        &game.fixtures.dataContext.buildingRegistry->Get("farm_booster"));
    pBase->GetProduction().SetMineralStockpile(9);
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    REQUIRE(game.pAi->GetBaseCount() == 0);

    DiplomaticProposal_t proposal;
    proposal.proposer = game.pPlayer->GetFactionId();
    proposal.recipient = game.pAi->GetFactionId();
    proposal.give.push_back(TradeBase_t{baseId});

    CHECK(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
          == DiplomaticProposeResult::Accepted);
    CHECK(game.pPlayer->GetBaseCount() == 0);
    REQUIRE(game.pAi->GetBaseCount() == 1);
    const BaseManager& rTransferred = *game.pAi->Bases().begin();
    CHECK(rTransferred.GetFactionId() == game.pAi->GetFactionId());
    CHECK(rTransferred.GetBaseId() == baseId);
    CHECK(rTransferred.GetName() == "Gift");
    CHECK(rTransferred.GetPopulation().GetSize() == popSize);
    CHECK(rTransferred.GetPopulation().GetNutrientStockpile() == 17);
    REQUIRE(rTransferred.GetBuildingManager().GetBuildings().size() == 1);
    CHECK(rTransferred.GetBuildingManager().GetBuildings()[0]->id == "flat_nutrient");
    REQUIRE(rTransferred.GetProduction().GetCurrentProduction() != nullptr);
    CHECK(rTransferred.GetProduction().GetCurrentProduction()->GetId() == "farm_booster");
    CHECK(rTransferred.GetProduction().GetMineralStockpile() == 9);
}
