#include "DiplomacyFixture.h"

#include "game/faction/DiplomaticActionExecutor.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionTileMemory.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/TradeItem.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionManager.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>

using namespace ac;
using namespace actest;

namespace
{

DiplomaticProposal_t Proposal_(const Faction& rProposer, const Faction& rRecipient)
{
    DiplomaticProposal_t proposal;
    proposal.proposer = rProposer.GetFactionId();
    proposal.recipient = rRecipient.GetFactionId();
    return proposal;
}

DiplomaticProposeResult_t Propose_(DiplomacyFixture& rGame, const DiplomaticProposal_t& rProposal)
{
    return rGame.pState->GetDiplomaticActionExecutor().Propose(*rGame.pState, rProposal);
}

} // namespace

TEST_CASE("Propose treaty to AI is accepted and applied", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.requestedStatus = DiplomaticStatus_t::Treaty;

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("A proposal may only request the next status up", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);

    proposal.requestedStatus = DiplomaticStatus_t::Truce;
    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Invalid);
    proposal.requestedStatus = DiplomaticStatus_t::Pact;
    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Invalid);
    // Declaring Vendetta and cancelling are one-sided, not offers.
    proposal.requestedStatus = DiplomaticStatus_t::Vendetta;
    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Invalid);

    game.Ledger().SetStatus(proposal.proposer, proposal.recipient, DiplomaticStatus_t::Treaty);
    proposal.requestedStatus = DiplomaticStatus_t::Neutral;
    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Invalid);
    proposal.requestedStatus = DiplomaticStatus_t::Pact;
    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Pact);
}

TEST_CASE("An empty proposal is invalid", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();

    CHECK(Propose_(game, Proposal_(*game.pA, *game.pB)) == DiplomaticProposeResult_t::Invalid);
    // Nor does it take the player's one pending slot.
    CHECK(Propose_(game, Proposal_(*game.pB, *game.pA)) == DiplomaticProposeResult_t::Invalid);
    CHECK_FALSE(game.pState->GetDiplomaticActionExecutor().GetPendingProposal().has_value());
}

TEST_CASE("Propose to player stays pending until Accept", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    DiplomaticProposal_t proposal = Proposal_(*game.pB, *game.pA);
    proposal.requestedStatus = DiplomaticStatus_t::Treaty;

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::PendingPlayer);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Neutral);

    REQUIRE(game.pState->GetDiplomaticActionExecutor().Accept(*game.pState));
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Energy credits trade moves treasury", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    game.pA->GetEconomy().AddEnergy(50);
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeCredits_t{20});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pA->GetEconomy().GetEnergy() == 30);
    CHECK(game.pB->GetEconomy().GetEnergy() == 20);
}

TEST_CASE("A Truce can be bought with credits", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    game.pA->GetEconomy().AddEnergy(10);
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.requestedStatus = DiplomaticStatus_t::Truce;
    proposal.give.push_back(TradeCredits_t{5});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Truce);
    CHECK(game.pA->GetEconomy().GetEnergy() == 5);
    CHECK(game.pB->GetEconomy().GetEnergy() == 5);
}

TEST_CASE("Technology trade grants tech to recipient", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    game.pA->GetResearch().AddDiscoveredTech("test_tech");
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeTechnology_t{"test_tech"});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pB->GetResearch().HasDiscoveredTech("test_tech"));
}

TEST_CASE("The same technology cannot be offered twice in one proposal", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    game.pA->GetResearch().AddDiscoveredTech("test_tech");
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeTechnology_t{"test_tech"});
    proposal.give.push_back(TradeTechnology_t{"test_tech"});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Invalid);
    CHECK_FALSE(game.pB->GetResearch().HasDiscoveredTech("test_tech"));
}

TEST_CASE("Comm frequency introduces third faction", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    game.Ledger().SetKnown(game.pB->GetFactionId(), game.pC->GetFactionId(), false);
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeCommFrequency_t{game.pC->GetFactionId()});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.Ledger().AreKnown(game.pB->GetFactionId(), game.pC->GetFactionId()));
}

TEST_CASE("A Vendetta offered in a trade is declared by the side that gives it",
          "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.demand.push_back(TradeDeclareVendetta_t{game.pC->GetFactionId()});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.Status(*game.pB, *game.pC) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Neutral);

    proposal.demand.clear();
    proposal.give.push_back(TradeDeclareVendetta_t{game.pC->GetFactionId()});
    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("World map trade merges explored tiles", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    game.pA->GetExploredMap().Mark(game.At(8, 6));
    REQUIRE(game.pA->GetExploredMap().IsExplored(8, 6));
    REQUIRE_FALSE(game.pB->GetExploredMap().IsExplored(8, 6));
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeWorldMap_t{});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pB->GetExploredMap().IsExplored(8, 6));
}

TEST_CASE("World map trade records the tiles it newly explores for the receiver",
          "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    const ImprovementConfig_t& rMine = game.fixtures.improvements.Get("Mine");
    Tile& rTraded = game.At(8, 6);
    Tile& rKnown = game.At(6, 6);
    game.pA->GetExploredMap().Mark(rTraded);
    game.pA->GetExploredMap().Mark(rKnown);
    game.pB->GetExploredMap().Mark(rKnown);
    REQUIRE_FALSE(game.pB->GetExploredMap().IsExplored(rTraded));
    rTraded.AddImprovement(rMine);
    rKnown.AddImprovement(rMine);
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeWorldMap_t{});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);

    const FactionTileMemory& rMemory = game.pB->GetTileMemory();
    REQUIRE(rMemory.Occupants(rTraded).improvements.size() == 1);
    CHECK(rMemory.Occupants(rTraded).improvements.front() == &rMine);
    CHECK(rMemory.Occupants(rKnown).improvements.empty());
}

TEST_CASE("Base transfer changes ownership", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    BaseManager& rBase = *game.pBaseA;
    const BaseId_t baseId = rBase.GetBaseId();
    const std::string name = rBase.GetName();
    const int popSize = rBase.GetPopulation().GetSize();
    rBase.GetBuildingManager().AddBuilding("flat_nutrient");
    const std::size_t buildingCount = rBase.GetBuildingManager().GetBuildings().size();
    rBase.GetPopulation().SetNutrientStockpile(17);
    rBase.GetProduction().SetProduction(
        &game.fixtures.dataContext.buildingRegistry->Get("farm_booster"), rBase.GetBaseEffects());
    rBase.GetProduction().SetMineralStockpile(9);
    const int receiverBases = game.pB->GetBaseCount();
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeBase_t{baseId});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pA->GetBaseCount() == 0);
    CHECK(game.pB->GetBaseCount() == receiverBases + 1);
    const BaseManager* pTransferred = game.pB->FindBase(baseId);
    REQUIRE(pTransferred);
    CHECK(pTransferred->GetFactionId() == game.pB->GetFactionId());
    CHECK(pTransferred->GetName() == name);
    CHECK(pTransferred->GetPopulation().GetSize() == popSize);
    CHECK(pTransferred->GetPopulation().GetNutrientStockpile() == 17);
    CHECK(pTransferred->GetBuildingManager().HasBuilding("Headquarters"));
    CHECK(pTransferred->GetBuildingManager().HasBuilding("flat_nutrient"));
    CHECK(pTransferred->GetBuildingManager().GetBuildings().size() == buildingCount);
    REQUIRE(pTransferred->GetProduction().GetCurrentProduction() != nullptr);
    CHECK(pTransferred->GetProduction().GetCurrentProduction()->GetId() == "farm_booster");
    CHECK(pTransferred->GetProduction().GetMineralStockpile() == 9);
}

TEST_CASE("The same base cannot be offered twice in one proposal", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeBase_t{game.pBaseA->GetBaseId()});
    proposal.give.push_back(TradeBase_t{game.pBaseA->GetBaseId()});
    const int receiverBases = game.pB->GetBaseCount();

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Invalid);
    CHECK(game.pA->GetBaseCount() == 1);
    CHECK(game.pB->GetBaseCount() == receiverBases);
}

TEST_CASE("A proposal is validated against its aggregate cost, not per item",
          "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    game.pA->GetEconomy().AddEnergy(60);
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeCredits_t{50});
    proposal.give.push_back(TradeCredits_t{50});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Invalid);
    CHECK(game.pA->GetEconomy().GetEnergy() == 60);
    CHECK(game.pB->GetEconomy().GetEnergy() == 0);
}

TEST_CASE("Two affordable credit items in one proposal still go through",
          "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    game.pA->GetEconomy().AddEnergy(60);
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeCredits_t{20});
    proposal.give.push_back(TradeCredits_t{30});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pA->GetEconomy().GetEnergy() == 10);
    CHECK(game.pB->GetEconomy().GetEnergy() == 50);
}

TEST_CASE("Each side of a proposal is costed against its own giver", "[diplomacy][executor]")
{
    // give and demand run in opposite directions; the aggregate must not be charged to one side.
    DiplomacyFixture game;
    game.MeetAll();
    game.pA->GetEconomy().AddEnergy(30);
    game.pB->GetEconomy().AddEnergy(30);
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeCredits_t{25});
    proposal.demand.push_back(TradeCredits_t{25});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pA->GetEconomy().GetEnergy() == 30);
    CHECK(game.pB->GetEconomy().GetEnergy() == 30);
}

TEST_CASE("A second proposal to the player is refused, not silently dropped",
          "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    DiplomaticProposal_t first = Proposal_(*game.pB, *game.pA);
    first.requestedStatus = DiplomaticStatus_t::Treaty;
    DiplomaticProposal_t second = Proposal_(*game.pC, *game.pA);
    second.requestedStatus = DiplomaticStatus_t::Treaty;

    DiplomaticActionExecutor& rExecutor = game.pState->GetDiplomaticActionExecutor();
    REQUIRE(rExecutor.Propose(*game.pState, first) == DiplomaticProposeResult_t::PendingPlayer);
    CHECK(rExecutor.Propose(*game.pState, second) == DiplomaticProposeResult_t::Busy);

    // The first proposal is intact and is what Accept resolves.
    REQUIRE(rExecutor.GetPendingProposal().has_value());
    CHECK(rExecutor.GetPendingProposal()->proposer == game.pB->GetFactionId());
    REQUIRE(rExecutor.Accept(*game.pState));
    CHECK(game.Status(*game.pB, *game.pA) == DiplomaticStatus_t::Treaty);
    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Neutral);

    // The slot is free again once the player answers.
    CHECK(rExecutor.Propose(*game.pState, second) == DiplomaticProposeResult_t::PendingPlayer);
}

TEST_CASE("Rejecting a pending proposal frees the slot without applying it",
          "[diplomacy][executor]")
{
    // Reject is the other half of the one-slot contract Busy relies on: without it a declined
    // proposal would block every later one forever.
    DiplomacyFixture game;
    game.MeetAll();
    game.pB->GetEconomy().AddEnergy(40);
    DiplomaticProposal_t proposal = Proposal_(*game.pB, *game.pA);
    proposal.give.push_back(TradeCredits_t{40});

    DiplomaticActionExecutor& rExecutor = game.pState->GetDiplomaticActionExecutor();
    REQUIRE(rExecutor.Propose(*game.pState, proposal) == DiplomaticProposeResult_t::PendingPlayer);

    rExecutor.Reject();
    CHECK_FALSE(rExecutor.GetPendingProposal().has_value());
    // Nothing moved.
    CHECK(game.pB->GetEconomy().GetEnergy() == 40);
    CHECK(game.pA->GetEconomy().GetEnergy() == 0);
    // Accepting a rejected proposal is a no-op, not a replay.
    CHECK_FALSE(rExecutor.Accept(*game.pState));
    // The slot is usable again.
    CHECK(rExecutor.Propose(*game.pState, proposal) == DiplomaticProposeResult_t::PendingPlayer);
}
