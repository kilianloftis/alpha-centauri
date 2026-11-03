#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/base/BaseManager.h"
#include "game/diplomacy/BaseValueRules.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticGiftRules.h"
#include "game/economy/EconomyManager.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>

using namespace ac;
using namespace actest;

namespace
{

// The player gives to an AI. Both treasuries start empty on turn 0, so every credit threshold
// but notable's minimum is 0.
struct GiftFixture
{
    FactionFixture fixtures;
    EffectPool pool;
    FactionConfig_t giverDefinition = fixtures.factionDefinition;
    FactionConfig_t recipientDefinition = fixtures.factionDefinition;
    Faction* pGiver = nullptr;
    Faction* pRecipient = nullptr;

    // Weighs rDefinition's gifts of kind by factor on stat.
    void Weigh(FactionConfig_t& rDefinition, StatId_t stat, TradeKind_t kind, double factor)
    {
        rDefinition.effects.push_back(pool.StatMod(stat, factor, ModifierOp_t::MultiplyGeometric,
                                                   EffectScope_t::FactionGlobal, std::nullopt,
                                                   Condition_t{GiftKind_t{kind}}));
    }

    // Call after any Weigh.
    void Seat()
    {
        pGiver = &fixtures.MakeFaction(giverDefinition);
        pRecipient = &fixtures.MakeFaction(recipientDefinition);
        REQUIRE(pGiver->IsPlayerControlled());
        REQUIRE_FALSE(pRecipient->IsPlayerControlled());
        Ledger().SetBlemishes(pGiver->GetFactionId(), 5);
    }

    DiplomacyLedger& Ledger() { return fixtures.state.GetDiplomacyLedger(); }

    std::optional<GiftStanding_t> Standing(const TradeItem_t& rItem)
    {
        return GiftStanding(pGiver->GetFactionId(), pRecipient->GetFactionId(), rItem);
    }

    int Friction(const TradeItem_t& rItem)
    {
        const std::optional<GiftStanding_t> standing = Standing(rItem);
        REQUIRE(standing);
        return standing->frictionDelta;
    }

    int Gifts(const TradeItem_t& rItem)
    {
        const std::optional<GiftStanding_t> standing = Standing(rItem);
        REQUIRE(standing);
        return standing->gifts;
    }
};

// fixtures/techs_full.json: a root tech weighted 2 on tech, which a tech-priority AI values at 4.
const TechId k_RootTech = "discover_tech";

} // namespace

TEST_CASE("A gift only gives: no status, nothing asked back", "[diplomacy][gifts]")
{
    DiplomaticProposal_t gift;
    gift.give.push_back(TradeCredits_t{50});
    CHECK(IsGift(gift));

    DiplomaticProposal_t trade = gift;
    trade.demand.push_back(TradeWorldMap_t{});
    CHECK_FALSE(IsGift(trade));

    DiplomaticProposal_t treaty = gift;
    treaty.requestedStatus = DiplomaticStatus_t::Treaty;
    CHECK_FALSE(IsGift(treaty));

    CHECK_FALSE(IsGift(DiplomaticProposal_t{}));
}

TEST_CASE("Credits lower the recipient's friction by tiers and count by both treasuries",
          "[diplomacy][gifts]")
{
    GiftFixture f;
    f.Seat();

    CHECK(f.Friction(TradeCredits_t{100}) == -3);
    CHECK(f.Gifts(TradeCredits_t{100}) == 2);

    // Half the recipient's 300 is 150, so 100 credits is only notable.
    f.pRecipient->GetEconomy().AddEnergy(300);
    CHECK(f.Gifts(TradeCredits_t{100}) == 1);
    f.Ledger().SetGifts(f.pGiver->GetFactionId(), f.pRecipient->GetFactionId(), 1);
    CHECK(f.Gifts(TradeCredits_t{100}) == 1);
}

TEST_CASE("The giver's credit weight doubles its credits and nothing else",
          "[diplomacy][gifts][effects]")
{
    GiftFixture f;
    SECTION("unweighted")
    {
        f.Seat();
        CHECK(f.Friction(TradeCredits_t{50}) == -1);
    }
    SECTION("weighted on credits")
    {
        f.Weigh(f.giverDefinition, StatId_t::GiftGivenValue, TradeKind_t::Credits, 2.0);
        f.Seat();
        CHECK(f.Friction(TradeCredits_t{50}) == -3);
    }
    SECTION("weighted on technology")
    {
        f.Weigh(f.giverDefinition, StatId_t::GiftGivenValue, TradeKind_t::Technology, 2.0);
        f.Seat();
        CHECK(f.Friction(TradeCredits_t{50}) == -1);
    }
}

TEST_CASE("Each gift weight truncates before the next", "[diplomacy][gifts][effects]")
{
    GiftFixture f;
    f.Weigh(f.giverDefinition, StatId_t::GiftGivenValue, TradeKind_t::Credits, 0.5);
    f.Weigh(f.recipientDefinition, StatId_t::GiftReceivedValue, TradeKind_t::Credits, 2.0);
    f.Seat();

    // 65 halves to 32 and doubles to 64, one unit short of 65's tier.
    CHECK(f.Friction(TradeCredits_t{65}) == -1);
}

TEST_CASE("Technology lowers friction by its value to the recipient", "[diplomacy][gifts]")
{
    GiftFixture f;
    f.recipientDefinition.ai.tech = true;
    f.Seat();
    f.pGiver->GetResearch().AddDiscoveredTech(k_RootTech);

    CHECK(f.Friction(TradeTechnology_t{k_RootTech}) == -4);
    // Short of 16, a tech gift is notable: it counts only from 0.
    CHECK(f.Gifts(TradeTechnology_t{k_RootTech}) == 1);
}

TEST_CASE("An AI values a tech less when more factions own it or it knows a deeper one",
          "[diplomacy][gifts]")
{
    GiftFixture f;
    f.recipientDefinition.ai.tech = true;
    f.Seat();
    f.pGiver->GetResearch().AddDiscoveredTech(k_RootTech);

    Faction& rThird = f.fixtures.MakeFaction();
    rThird.GetResearch().AddDiscoveredTech(k_RootTech);
    CHECK(f.Friction(TradeTechnology_t{k_RootTech}) == -2);

    // quantum_power is level 2, so the root tech scales by (1 + 1) / (2 + 1): 2 x 2 / 3.
    f.pRecipient->GetResearch().AddDiscoveredTech("quantum_power");
    CHECK(f.Friction(TradeTechnology_t{k_RootTech}) == -1);
}

TEST_CASE("The recipient's tech weight doubles a tech it receives, not one its giver holds",
          "[diplomacy][gifts][effects]")
{
    GiftFixture f;
    f.recipientDefinition.ai.tech = true;
    SECTION("on the recipient")
    {
        f.Weigh(f.recipientDefinition, StatId_t::GiftReceivedValue, TradeKind_t::Technology, 2.0);
        f.Seat();
        CHECK(f.Friction(TradeTechnology_t{k_RootTech}) == -8);
    }
    SECTION("on the giver")
    {
        f.Weigh(f.giverDefinition, StatId_t::GiftReceivedValue, TradeKind_t::Technology, 2.0);
        f.Seat();
        CHECK(f.Friction(TradeTechnology_t{k_RootTech}) == -4);
    }
}

TEST_CASE("A base lowers friction by half again its value to the recipient",
          "[diplomacy][gifts][base_value]")
{
    GiftFixture f;
    f.Seat();
    BaseManager& rGiven = f.fixtures.MakeFactionBase(*f.pGiver, 18, 8);
    const int value = BaseValue(rGiven, *f.pRecipient, *f.pGiver);

    CHECK(f.Friction(TradeBase_t{rGiven.GetBaseId()}) == -(value * 3 / 2));
    CHECK(f.Gifts(TradeBase_t{rGiven.GetBaseId()}) == 2);
}

TEST_CASE("A base short of generous still counts once at any count",
          "[diplomacy][gifts][base_value]")
{
    GiftFixture f;
    f.Seat();
    BaseManager& rGiven = f.fixtures.MakeFactionBase(*f.pGiver, 18, 8);
    f.pRecipient->GetEconomy().AddEnergy(100000);
    REQUIRE(BaseValue(rGiven, *f.pRecipient, *f.pGiver) < 100000 / 2);
    f.Ledger().SetGifts(f.pGiver->GetFactionId(), f.pRecipient->GetFactionId(), 1);

    CHECK(f.Gifts(TradeBase_t{rGiven.GetBaseId()}) == 2);
}

TEST_CASE("The giver's size threshold makes a base generous without moving its friction",
          "[diplomacy][gifts][effects][base_value]")
{
    GiftFixture f;
    // The value sits between a quarter and half of the recipient's treasury.
    const auto giveBase = [&f](int recipientCredits)
    {
        BaseManager& rGiven = f.fixtures.MakeFactionBase(*f.pGiver, 18, 8);
        f.pRecipient->GetEconomy().AddEnergy(recipientCredits);
        return TradeBase_t{rGiven.GetBaseId()};
    };
    SECTION("unscaled")
    {
        f.Seat();
        const TradeBase_t base = giveBase(300);
        const int value = BaseValue(*f.pGiver->FindBase(base.baseId), *f.pRecipient, *f.pGiver);
        REQUIRE(value < 300 / 2);
        REQUIRE(value >= 300 / 4);
        CHECK(f.Gifts(base) == 1);
    }
    SECTION("halved")
    {
        f.Weigh(f.giverDefinition, StatId_t::GiftSizeThreshold, TradeKind_t::Base, 0.5);
        f.Seat();
        const TradeBase_t base = giveBase(300);
        const int value = BaseValue(*f.pGiver->FindBase(base.baseId), *f.pRecipient, *f.pGiver);
        CHECK(f.Gifts(base) == 2);
        CHECK(f.Friction(base) == -(value * 3 / 2));
    }
    SECTION("zeroed")
    {
        f.Weigh(f.giverDefinition, StatId_t::GiftSizeThreshold, TradeKind_t::Base, 0.0);
        f.Seat();
        CHECK(f.Gifts(giveBase(100000)) == 2);
    }
}

TEST_CASE("Comm frequencies, maps and Vendettas carry no gift standing", "[diplomacy][gifts]")
{
    GiftFixture f;
    f.Seat();

    CHECK_FALSE(f.Standing(TradeCommFrequency_t{f.pRecipient->GetFactionId()}));
    CHECK_FALSE(f.Standing(TradeWorldMap_t{}));
    CHECK_FALSE(f.Standing(TradeDeclareVendetta_t{f.pRecipient->GetFactionId()}));
}
