#include "RecordingGraphics.h"
#include "ViewFixture.h"

#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStatus.h"
#include "game/faction/Faction.h"
#include "ui/commlinks/CommlinksPanel.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;

namespace
{

// A rival with a name of its own, so its row can be found.
struct CommlinksHarness_
{
    ViewFixture fixture;
    FactionConfig_t rivalDefinition = RivalDefinition_(fixture);
    Faction& rRival = fixture.state.AddFaction(std::make_unique<Faction>(
        fixture.state.AllocateFactionId(), /*bIsPlayerControlled*/ false, rivalDefinition,
        fixture.dataContext, fixture.state.GetWorldMap(), fixture.settings,
        fixture.state.GetUnitIdAllocator(), actest::k_TestFactionSeed));

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

    RecordingGraphics Render()
    {
        CommlinksPanel panel(fixture.state, *fixture.pStandingCaptions, ViewFixture::FullScreen());
        RecordingGraphics graphics;
        panel.Render(graphics);
        return graphics;
    }
};

// Every text drawn on the rival's row, in draw order.
std::vector<std::string> RivalRow_(const RecordingGraphics& rGraphics)
{
    const auto itName = std::ranges::find(rGraphics.texts, std::string("Rival"),
                                          &RecordingGraphics::TextDraw_t::text);
    REQUIRE(itName != rGraphics.texts.end());
    std::vector<std::string> row;
    for (const RecordingGraphics::TextDraw_t& rText : rGraphics.texts)
    {
        if (rText.y == itName->y)
        {
            row.push_back(rText.text);
        }
    }
    return row;
}

} // namespace

TEST_CASE("A met faction's row shows its mood toward the player and its repute",
          "[ui][commlinks]")
{
    CommlinksHarness_ harness;
    harness.Ledger().SetKnown(harness.PlayerId(), harness.RivalId(), true);
    harness.Ledger().SetFriction(harness.RivalId(), harness.PlayerId(), 9);
    harness.Ledger().SetBlemishes(harness.RivalId(), 4);

    const RecordingGraphics graphics = harness.Render();

    CHECK(RivalRow_(graphics) == std::vector<std::string>{"Rival", "Ambivalent", "Ruthless"});
    CHECK(graphics.AnyTextContaining("Mood"));
    CHECK(graphics.AnyTextContaining("Reputation"));
}

TEST_CASE("The player's friction toward a faction does not change that faction's row",
          "[ui][commlinks]")
{
    CommlinksHarness_ harness;
    harness.Ledger().SetKnown(harness.PlayerId(), harness.RivalId(), true);
    harness.Ledger().SetFriction(harness.PlayerId(), harness.RivalId(), 20);

    const RecordingGraphics graphics = harness.Render();

    CHECK(RivalRow_(graphics) == std::vector<std::string>{"Rival", "Magnanimous", "Noble"});
}

TEST_CASE("At Vendetta a row's mood is its stored friction, with no floor", "[ui][commlinks]")
{
    CommlinksHarness_ harness;
    harness.Ledger().SetKnown(harness.PlayerId(), harness.RivalId(), true);
    harness.Ledger().SetStatus(harness.PlayerId(), harness.RivalId(),
                               DiplomaticStatus_t::Vendetta);

    const RecordingGraphics graphics = harness.Render();

    const std::string vendetta = ToString(DiplomaticStatus_t::Vendetta);
    CHECK(RivalRow_(graphics)
          == std::vector<std::string>{"Rival", vendetta, "Magnanimous", "Noble"});
}

TEST_CASE("A faction the player has not met has no row", "[ui][commlinks]")
{
    CommlinksHarness_ harness;

    const RecordingGraphics graphics = harness.Render();

    CHECK_FALSE(graphics.AnyTextContaining("Rival"));
}

TEST_CASE("The integrity line names the player's own repute", "[ui][commlinks]")
{
    CommlinksHarness_ harness;
    harness.Ledger().SetBlemishes(harness.PlayerId(), 6);
    harness.Ledger().SetBlemishes(harness.RivalId(), 1);

    const RecordingGraphics graphics = harness.Render();

    CHECK(graphics.AnyTextContaining("Integrity: Wicked"));
}
