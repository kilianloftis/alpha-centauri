#include "DiplomacyFixture.h"

#include "game/diplomacy/DiplomaticActionExecutor.h"
#include "game/diplomacy/DiplomaticProposalRules.h"
#include "game/economy/EconomyManager.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/visibility/FactionTileMemory.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/research/ResearchManager.h"
#include "game/diplomacy/TradeItem.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/production/ProductionManager.h"
#include "game/buildings/BuildingRegistry.h"

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
    return rGame.fixtures.state.GetDiplomaticActionExecutor().Propose(rGame.fixtures.state, rProposal);
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
    CHECK_FALSE(game.fixtures.state.GetDiplomaticActionExecutor().GetPendingProposal().has_value());
}

TEST_CASE("Propose to player stays pending until Accept", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    DiplomaticProposal_t proposal = Proposal_(*game.pB, *game.pA);
    proposal.requestedStatus = DiplomaticStatus_t::Treaty;

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::PendingPlayer);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Neutral);

    REQUIRE(game.fixtures.state.GetDiplomaticActionExecutor().Accept(game.fixtures.state));
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
    game.pA->GetResearch().AddDiscoveredTech("build_tech");
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeTechnology_t{"build_tech"});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pB->GetResearch().HasDiscoveredTech("build_tech"));
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

TEST_CASE("Base transfer changes ownership and destroys a Headquarters", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    BaseManager& rBase = *game.pBaseA;
    rBase.GetBuildingManager().AddBuilding("Headquarters");
    const BaseId_t baseId = rBase.GetBaseId();
    const std::string name = rBase.GetName();
    const int popSize = rBase.GetPopulation().GetSize();
    rBase.GetBuildingManager().AddBuilding("flat_nutrient");
    const std::size_t buildingCount = rBase.GetBuildingManager().GetBuildings().size();
    rBase.GetPopulation().SetNutrientStockpile(17);
    rBase.GetProduction().SetProduction(
        &game.fixtures.dataContext.buildingRegistry->Get("farm_booster"), rBase.GetBaseEffects());
    rBase.GetProduction().SetMineralStockpile(9);
    MakeSessionBase(game.fixtures, *game.pA, 20, 6);
    const int receiverBases = game.pB->GetBaseCount();
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeBase_t{baseId});

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pA->GetBaseCount() == 1);
    CHECK(game.pB->GetBaseCount() == receiverBases + 1);
    const BaseManager* pTransferred = game.pB->FindBase(baseId);
    REQUIRE(pTransferred);
    CHECK(pTransferred->GetFactionId() == game.pB->GetFactionId());
    CHECK(pTransferred->GetName() == name);
    CHECK(pTransferred->GetPopulation().GetSize() == popSize);
    CHECK(pTransferred->GetPopulation().GetNutrientStockpile() == 17);
    CHECK_FALSE(pTransferred->GetBuildingManager().HasBuilding("Headquarters"));
    CHECK(pTransferred->GetBuildingManager().HasBuilding("flat_nutrient"));
    CHECK(pTransferred->GetBuildingManager().GetBuildings().size() == buildingCount - 1);
    REQUIRE(pTransferred->GetProduction().GetCurrentProduction() != nullptr);
    CHECK(pTransferred->GetProduction().GetCurrentProduction()->GetId() == "farm_booster");
    CHECK(pTransferred->GetProduction().GetMineralStockpile() == 9);
}

TEST_CASE("The same base cannot be offered twice in one proposal", "[diplomacy][executor]")
{
    DiplomacyFixture game;
    game.MeetAll();
    MakeSessionBase(game.fixtures, *game.pA, 20, 6);
    DiplomaticProposal_t proposal = Proposal_(*game.pA, *game.pB);
    proposal.give.push_back(TradeBase_t{game.pBaseA->GetBaseId()});
    proposal.give.push_back(TradeBase_t{game.pBaseA->GetBaseId()});
    const int receiverBases = game.pB->GetBaseCount();

    CHECK(Propose_(game, proposal) == DiplomaticProposeResult_t::Invalid);
    CHECK(game.pA->GetBaseCount() == 2);
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

    DiplomaticActionExecutor& rExecutor = game.fixtures.state.GetDiplomaticActionExecutor();
    REQUIRE(rExecutor.Propose(game.fixtures.state, first) == DiplomaticProposeResult_t::PendingPlayer);
    CHECK(rExecutor.Propose(game.fixtures.state, second) == DiplomaticProposeResult_t::Busy);

    // The first proposal is intact and is what Accept resolves.
    REQUIRE(rExecutor.GetPendingProposal().has_value());
    CHECK(rExecutor.GetPendingProposal()->proposer == game.pB->GetFactionId());
    REQUIRE(rExecutor.Accept(game.fixtures.state));
    CHECK(game.Status(*game.pB, *game.pA) == DiplomaticStatus_t::Treaty);
    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Neutral);

    // The slot is free again once the player answers.
    CHECK(rExecutor.Propose(game.fixtures.state, second) == DiplomaticProposeResult_t::PendingPlayer);
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

    DiplomaticActionExecutor& rExecutor = game.fixtures.state.GetDiplomaticActionExecutor();
    REQUIRE(rExecutor.Propose(game.fixtures.state, proposal) == DiplomaticProposeResult_t::PendingPlayer);

    rExecutor.Reject();
    CHECK_FALSE(rExecutor.GetPendingProposal().has_value());
    // Nothing moved.
    CHECK(game.pB->GetEconomy().GetEnergy() == 40);
    CHECK(game.pA->GetEconomy().GetEnergy() == 0);
    // Accepting a rejected proposal is a no-op, not a replay.
    CHECK_FALSE(rExecutor.Accept(game.fixtures.state));
    // The slot is usable again.
    CHECK(rExecutor.Propose(game.fixtures.state, proposal) == DiplomaticProposeResult_t::PendingPlayer);
}

TEST_CASE("CanDeliver answers for one item from its giver", "[diplomacy][proposal]")
{
    DiplomacyFixture game;
    game.MeetAll();
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    const FactionId_t c = game.pC->GetFactionId();
    game.pA->GetEconomy().AddEnergy(10);
    game.pA->GetResearch().AddDiscoveredTech("test_tech");
    MakeSessionBase(game.fixtures, *game.pA, 20, 6);

    CHECK(CanDeliver(a, b, TradeCredits_t{10}));
    CHECK_FALSE(CanDeliver(a, b, TradeCredits_t{11}));
    CHECK(CanDeliver(a, b, TradeTechnology_t{"test_tech"}));
    CHECK_FALSE(CanDeliver(b, a, TradeTechnology_t{"test_tech"}));
    CHECK(CanDeliver(a, b, TradeBase_t{static_cast<BaseId_t>(game.pBaseA->GetBaseId())}));
    CHECK_FALSE(CanDeliver(b, a, TradeBase_t{static_cast<BaseId_t>(game.pBaseA->GetBaseId())}));
    CHECK(CanDeliver(a, b, TradeWorldMap_t{}));
    CHECK(CanDeliver(a, b, TradeDeclareVendetta_t{c}));
    CHECK_FALSE(CanDeliver(a, b, TradeDeclareVendetta_t{b}));

    // B already knows C, so A has no frequency to hand over until B forgets it.
    CHECK_FALSE(CanDeliver(a, b, TradeCommFrequency_t{c}));
    game.Ledger().SetKnown(b, c, false);
    CHECK(CanDeliver(a, b, TradeCommFrequency_t{c}));
}

TEST_CASE("A faction never hands over its last base", "[diplomacy][proposal]")
{
    DiplomacyFixture game;
    game.MeetAll();
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    const TradeBase_t only{game.pBaseA->GetBaseId()};
    REQUIRE(game.pA->GetBaseCount() == 1);

    CHECK_FALSE(CanDeliver(a, b, only));
    DiplomaticProposal_t gift = Proposal_(*game.pA, *game.pB);
    gift.give.push_back(only);
    CHECK(Propose_(game, gift) == DiplomaticProposeResult_t::Invalid);
    CHECK(game.pA->GetBaseCount() == 1);
}

TEST_CASE("One side never hands over every base its giver holds", "[diplomacy][proposal]")
{
    DiplomacyFixture game;
    game.MeetAll();
    BaseManager& rSecond = MakeSessionBase(game.fixtures, *game.pA, 20, 6);
    DiplomaticProposal_t both = Proposal_(*game.pA, *game.pB);
    both.give = {TradeBase_t{game.pBaseA->GetBaseId()}, TradeBase_t{rSecond.GetBaseId()}};

    CHECK(Propose_(game, both) == DiplomaticProposeResult_t::Invalid);
    CHECK(game.pA->GetBaseCount() == 2);

    DiplomaticProposal_t one = Proposal_(*game.pA, *game.pB);
    one.give = {TradeBase_t{rSecond.GetBaseId()}};
    CHECK(Propose_(game, one) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pA->GetBaseCount() == 1);
}

TEST_CASE("A proposal may not demand the recipient's headquarters", "[diplomacy][proposal]")
{
    DiplomacyFixture game;
    game.MeetAll();
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    BaseManager& rOutpost = MakeSessionBase(game.fixtures, *game.pB, 20, 12);
    game.pBaseB->GetBuildingManager().AddBuilding("Headquarters");
    const TradeBase_t headquarters{game.pBaseB->GetBaseId()};
    const TradeBase_t outpost{rOutpost.GetBaseId()};

    CHECK(CanDeliver(b, a, headquarters));
    CHECK_FALSE(CanDemand(b, a, headquarters));
    CHECK(CanDemand(b, a, outpost));

    DiplomaticProposal_t demand = Proposal_(*game.pA, *game.pB);
    demand.demand = {headquarters};
    CHECK(Propose_(game, demand) == DiplomaticProposeResult_t::Invalid);

    demand.demand = {outpost};
    CHECK(Propose_(game, demand) == DiplomaticProposeResult_t::Accepted);
    CHECK(game.pA->FindBase(outpost.baseId) != nullptr);
}
