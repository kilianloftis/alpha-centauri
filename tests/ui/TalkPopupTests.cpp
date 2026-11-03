#include "RecordingGraphics.h"
#include "ViewFixture.h"

#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStatus.h"
#include "game/economy/EconomyManager.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "ui/commlinks/TalkPopup.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;

namespace
{

// A talk between the fixture's player and an AI rival whose words are in the fixture text pack.
struct TalkHarness_
{
    ViewFixture fixture;
    FactionConfig_t rivalDefinition = RivalDefinition_(fixture);
    Faction& rRival = fixture.state.AddFaction(std::make_unique<Faction>(
        fixture.state.AllocateFactionId(), /*bIsPlayerControlled*/ false, rivalDefinition,
        fixture.dataContext, fixture.state.GetWorldMap(), fixture.settings,
        fixture.state.GetUnitIdAllocator(), actest::k_TestFactionSeed));
    std::unique_ptr<TalkPopup> pPopup;

    TalkHarness_()
    {
        Ledger().SetKnown(PlayerId(), RivalId(), true);
    }

    static FactionConfig_t RivalDefinition_(const ViewFixture& rFixture)
    {
        FactionConfig_t definition = rFixture.factionDefinition;
        definition.id = "rival_faction";
        definition.identity.name = "Rival";
        return definition;
    }

    DiplomacyLedger& Ledger() { return fixture.state.GetDiplomacyLedger(); }
    FactionId_t PlayerId() const { return fixture.pPlayer->GetFactionId(); }
    FactionId_t RivalId() const { return rRival.GetFactionId(); }

    void Open()
    {
        pPopup = std::make_unique<TalkPopup>(fixture.state, *fixture.pTalkScript,
                                             *fixture.pTextPack, *fixture.pStandingCaptions,
                                             RivalId(), 1u, ViewFixture::FullScreen());
    }

    RecordingGraphics Render() const
    {
        RecordingGraphics graphics;
        pPopup->Render(graphics);
        return graphics;
    }

    bool Shows(const std::string& rText) const
    {
        const RecordingGraphics graphics = Render();
        return std::ranges::any_of(graphics.texts, [&rText](const auto& rDraw)
                                   { return rDraw.text == rText; });
    }

    // Clicks the menu row drawn as rLabel.
    void Choose(const std::string& rLabel)
    {
        const RecordingGraphics graphics = Render();
        const auto it = std::ranges::find_if(graphics.texts, [&rLabel](const auto& rDraw)
                                             { return rDraw.text == rLabel; });
        REQUIRE(it != graphics.texts.end());
        pPopup->HandleMouseClick(MouseEvent_t{MouseButton_t::Left, static_cast<int>(it->x) + 1,
                                              static_cast<int>(it->y) + 1, {}});
    }
};

} // namespace

TEST_CASE("Talk opens with the caption, the standing and the rival's greeting", "[ui][talk]")
{
    TalkHarness_ harness;
    harness.Ledger().SetStatus(harness.PlayerId(), harness.RivalId(), DiplomaticStatus_t::Vendetta);
    harness.Ledger().SetBlemishes(harness.RivalId(), 2);
    harness.Open();

    // The caption floors friction 0 at Vendetta; the portrait's mood stays raw.
    CHECK(harness.Shows("Rivals - Obstinate"));
    CHECK(harness.Shows("Mood"));
    CHECK(harness.Shows("Magnanimous"));
    CHECK(harness.Shows("Reputation"));
    CHECK(harness.Shows("Scrupulous"));
    CHECK(harness.Shows("Integrity"));
    CHECK(harness.Shows("Noble"));
    CHECK(harness.Shows("Hello Chief Tester, I am Riva of the Rivals."));
    CHECK(harness.Shows("My intent is rival intent; think of your tester projects."));
    CHECK(harness.Shows("Propose."));
    CHECK(harness.Shows("Goodbye, Tester."));
}

TEST_CASE("Proposing the next status sets it and plays its acceptance", "[ui][talk]")
{
    TalkHarness_ harness;
    harness.Open();
    harness.Choose("Propose.");
    CHECK_FALSE(harness.Shows("A Truce."));
    CHECK_FALSE(harness.Shows("A Brotherhood."));

    harness.Choose("A Treaty.");
    CHECK(harness.Ledger().GetStatus(harness.PlayerId(), harness.RivalId())
          == DiplomaticStatus_t::Treaty);
    CHECK(harness.Shows("Treaty agreed."));
    CHECK(harness.Shows("Anything else?"));
}

TEST_CASE("At Vendetta the proposal offered is a Truce", "[ui][talk]")
{
    TalkHarness_ harness;
    harness.Ledger().SetStatus(harness.PlayerId(), harness.RivalId(), DiplomaticStatus_t::Vendetta);
    harness.Open();
    harness.Choose("Propose.");
    CHECK_FALSE(harness.Shows("A Treaty."));

    harness.Choose("A Truce.");
    CHECK(harness.Ledger().GetStatus(harness.PlayerId(), harness.RivalId())
          == DiplomaticStatus_t::Truce);
    CHECK(harness.Shows("Truce agreed."));
}

TEST_CASE("Renouncing is offered only over a Pact", "[ui][talk]")
{
    TalkHarness_ harness;
    harness.Ledger().SetStatus(harness.PlayerId(), harness.RivalId(), DiplomaticStatus_t::Treaty);
    harness.Open();
    CHECK_FALSE(harness.Shows("Renounce our Brotherhood."));

    harness.Ledger().SetStatus(harness.PlayerId(), harness.RivalId(), DiplomaticStatus_t::Pact);
    harness.Open();
    CHECK(harness.Shows("Renounce our Brotherhood."));
}

TEST_CASE("Backing down from a renounce keeps the Pact", "[ui][talk]")
{
    TalkHarness_ harness;
    harness.Ledger().SetStatus(harness.PlayerId(), harness.RivalId(), DiplomaticStatus_t::Pact);
    harness.Open();
    harness.Choose("Renounce our Brotherhood.");
    CHECK(harness.Shows("Really, Tester?"));

    harness.Choose("No.");
    CHECK(harness.Ledger().GetStatus(harness.PlayerId(), harness.RivalId())
          == DiplomaticStatus_t::Pact);
    CHECK(harness.Shows("Good."));
}

TEST_CASE("Renouncing a Pact leaves a Treaty and blemishes the player", "[ui][talk]")
{
    TalkHarness_ harness;
    harness.Ledger().SetStatus(harness.PlayerId(), harness.RivalId(), DiplomaticStatus_t::Pact);
    harness.Open();
    harness.Choose("Renounce our Brotherhood.");
    harness.Choose("Yes.");

    CHECK(harness.Ledger().GetStatus(harness.PlayerId(), harness.RivalId())
          == DiplomaticStatus_t::Treaty);
    // The fixture's break_agreement_blemishes; SMAC adds one inline (terranx-map.md, Talk).
    CHECK(harness.Ledger().GetBlemishes(harness.PlayerId()) == 1);
    CHECK(harness.Ledger().GetBlemishes(harness.RivalId()) == 0);
    CHECK(harness.Shows("Farewell, false Brother."));
}

TEST_CASE("A traded tech moves to the rival", "[ui][talk]")
{
    TalkHarness_ harness;
    harness.fixture.pPlayer->GetResearch().AddDiscoveredTech("build_tech");
    harness.Open();
    harness.Choose("Propose.");
    harness.Choose("Trade.");
    CHECK_FALSE(harness.Shows("Deal."));

    harness.Choose("Offer Technology.");
    harness.Choose("Build Tech");
    CHECK(harness.Shows("You offer:"));
    CHECK(harness.Shows("Build Tech"));

    harness.Choose("Deal.");
    CHECK(harness.rRival.GetResearch().HasDiscoveredTech("build_tech"));
    CHECK(harness.Shows("Trade agreed."));
}

TEST_CASE("Gifted credits move to the rival and soften its mood", "[ui][talk][gifts]")
{
    TalkHarness_ harness;
    harness.fixture.pPlayer->GetEconomy().AddEnergy(100);
    harness.Ledger().SetFriction(harness.RivalId(), harness.PlayerId(), 3);
    harness.Open();
    CHECK(harness.Shows("Cooperative"));

    harness.Choose("Propose.");
    harness.Choose("Trade.");
    harness.Choose("Offer Credits.");
    harness.Choose("50 credits");
    harness.Choose("Deal.");

    CHECK(harness.rRival.GetEconomy().GetEnergy() == 50);
    CHECK(harness.Ledger().GetFriction(harness.RivalId(), harness.PlayerId()) == 2);
    CHECK(harness.Shows("Solicitous"));
}

TEST_CASE("Escape and goodbye close the talk without changing anything", "[ui][talk]")
{
    TalkHarness_ harness;
    harness.Open();
    harness.pPopup->HandleKey(KeyEvent_t{Key_t::Escape});
    CHECK(harness.pPopup->ShouldClose());

    harness.Open();
    harness.Choose("Goodbye, Tester.");
    CHECK(harness.pPopup->ShouldClose());
    CHECK(harness.Ledger().GetStatus(harness.PlayerId(), harness.RivalId())
          == DiplomaticStatus_t::Neutral);
}
