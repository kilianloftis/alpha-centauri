// Views built through the real ViewFactory against a live GameState — the coverage package 15
// commit A was missing: the factory's null policy, and two view-level rules that were putting
// wrong information on screen.

#include "ViewFixture.h"

#include "game/Faction.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/GameSettings.h"
#include "game/units/UnitDesign.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace ac;
using actest::ViewFixture;

TEST_CASE("A view that needs the player faction throws when there is none", "[ui][factory]")
{
    // These returned nullptr and PushView dereferenced whatever it was handed.
    ViewFixture fixture(/*bWithPlayerFaction*/ false);
    REQUIRE(fixture.pState->GetPlayerFaction() == nullptr);

    CHECK_THROWS_AS(fixture.pFactory->CreateResearchView(ViewFixture::FullScreen()),
                    std::runtime_error);
    CHECK_THROWS_AS(fixture.pFactory->CreateSocialEngineeringView(ViewFixture::FullScreen()),
                    std::runtime_error);
    CHECK_THROWS_AS(fixture.pFactory->CreateUnitDesignerView(ViewFixture::FullScreen()),
                    std::runtime_error);
}

TEST_CASE("Opening a base needs the player faction too", "[ui][factory]")
{
    // CreateBaseView reads the player faction to decide editability, and it is the view the
    // world-screen base click reaches.
    ViewFixture fixture(/*bWithPlayerFaction*/ false);
    ac::Faction& rOwner = fixture.pState->AddFaction(std::make_unique<ac::Faction>(
        fixture.pState->AllocateFactionId(), /*bIsPlayerControlled*/ false,
        fixture.factionDefinition, fixture.dataContext, fixture.pState->GetWorldMap(),
        fixture.settings, actest::k_TestFactionSeed));
    ac::BaseManager* pBase = rOwner.CreateBase(
        fixture.pState->AllocateBaseId(), "Rival", fixture.pState->GetWorldMap().GetTile(4, 4),
        fixture.dataContext, fixture.pState->GetTileEffects(),
        fixture.pState->GetSecretProjectAvailability());
    REQUIRE(pBase != nullptr);

    CHECK_THROWS_AS(fixture.pFactory->CreateBaseView(*pBase, ViewFixture::FullScreen()),
                    std::runtime_error);
}

TEST_CASE("The same views build when a player faction exists", "[ui][factory]")
{
    ViewFixture fixture;
    CHECK(fixture.pFactory->CreateResearchView(ViewFixture::FullScreen()) != nullptr);
    CHECK(fixture.pFactory->CreateSocialEngineeringView(ViewFixture::FullScreen()) != nullptr);
    CHECK(fixture.pFactory->CreateUnitDesignerView(ViewFixture::FullScreen()) != nullptr);
}

TEST_CASE("The unit designer hides slots gated behind undiscovered tech", "[ui][unit-designer]")
{
    // The designer laid out every slot in the registry regardless of requiredTech, so a player
    // could fill a slot the faction has not unlocked and save the design.
    ViewFixture fixture;
    REQUIRE_FALSE(fixture.pPlayer->GetResearch().HasDiscoveredTech("advanced_build"));

    auto pView = fixture.pFactory->CreateUnitDesignerView(ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);

    CHECK(fixture.graphics.AnyTextContaining("Chassis"));
    CHECK_FALSE(fixture.graphics.AnyTextContaining("Gated Ability Slot"));
}

TEST_CASE("The unit designer shows a gated slot once its tech is discovered",
          "[ui][unit-designer]")
{
    ViewFixture fixture;
    fixture.pPlayer->GetResearch().AddDiscoveredTech("advanced_build");

    auto pView = fixture.pFactory->CreateUnitDesignerView(ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);

    CHECK(fixture.graphics.AnyTextContaining("Gated Ability Slot"));
}

TEST_CASE("The faction bonus line reports the active rating's effects", "[ui][social]")
{
    // FormatFactionBonuses resolved each rating's level effects and then dropped them on the
    // floor without writing to the stream, so the line read "None" for every faction.
    ViewFixture fixture;
    // growth_policy gives +2 Growth; social_rating_effects.json maps Growth 2 to +1 nutrients
    // and +20% growth rate.
    fixture.pPlayer->GetSocialEngineering().SetActivePolicy(
        fixture.dataContext.socialPolicyRegistry->Get("growth_policy"));
    REQUIRE(fixture.pPlayer->GetSocialEngineering().GetSocialRating(SocialRatingId_t::Growth)
            == 2);

    auto pView = fixture.pFactory->CreateSocialEngineeringView(ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);

    // Exact, because "None" is also a legitimate policy name in the category rows: what is
    // being asserted is the content of the bonus line itself.
    CHECK_FALSE(fixture.graphics.TextYs("+1 Nutrients, +20% Growth Rate").empty());
}

namespace
{

// Click at a point just inside the first drawn string containing rNeedle. Using recorded draw
// positions as the hit-test oracle keeps the test from re-deriving the view's layout maths.
bool ClickDrawnText(IGameView& rView, const actest::RecordingGraphics& rGraphics,
                    const std::string& rNeedle)
{
    for (const auto& rDraw : rGraphics.texts)
    {
        if (rDraw.text.find(rNeedle) != std::string::npos)
        {
            MouseEvent_t event{MouseButton_t::Left, static_cast<int>(rDraw.x) + 2,
                               static_cast<int>(rDraw.y) + 2, {}, true};
            rView.HandleMouse(event);
            return true;
        }
    }
    return false;
}

// Fill one slot: click the slot row, then the first component in the popup that opens.
void FillSlot(IGameView& rView, actest::RecordingGraphics& rGraphics,
              const std::string& rSlotName, const std::string& rComponentName)
{
    rGraphics.texts.clear();
    rView.Render(rGraphics);
    REQUIRE(ClickDrawnText(rView, rGraphics, rSlotName));

    rGraphics.texts.clear();
    rView.Render(rGraphics);
    REQUIRE(ClickDrawnText(rView, rGraphics, rComponentName));
}

} // namespace

TEST_CASE("A design saves when every slot the player can see is filled", "[ui][unit-designer]")
{
    // The save gate, the Save button's enabled state and the saved UnitDesign were all built
    // from the *whole* slot registry while the columns were built from the unlocked subset. A
    // required slot behind an undiscovered tech was therefore unfillable and permanently
    // incomplete: Save was dead for the rest of the game, and UnitDesign's constructor throws
    // on a required slot with no component.
    ViewFixture fixture;
    REQUIRE_FALSE(fixture.pPlayer->GetResearch().HasDiscoveredTech("advanced_build"));

    auto pView = fixture.pFactory->CreateUnitDesignerView(ViewFixture::FullScreen());
    REQUIRE(pView);

    pView->Render(fixture.graphics);
    REQUIRE_FALSE(fixture.graphics.AnyTextContaining("Gated Ability Slot"));
    REQUIRE(fixture.graphics.AnyTextContaining("Fill all required slots"));

    FillSlot(*pView, fixture.graphics, "Chassis", "Test Chassis");
    FillSlot(*pView, fixture.graphics, "Weapon", "Test Weapon");
    FillSlot(*pView, fixture.graphics, "Armour", "Test Armor");

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK_FALSE(fixture.graphics.AnyTextContaining("Fill all required slots"));

    const size_t before = fixture.pPlayer->GetMilitary().GetDesigns().size();
    REQUIRE(ClickDrawnText(*pView, fixture.graphics, "Save"));
    CHECK(fixture.pPlayer->GetMilitary().GetDesigns().size() == before + 1);
}

TEST_CASE("The research panel shows the tech's name, not its config id", "[ui][research]")
{
    // The panel painted GetResearchTarget(), which is the wire-form id — the player read
    // "build_tech" where config carries "Build Tech".
    ViewFixture fixture;
    fixture.pPlayer->GetResearch().SetResearchTarget("build_tech");

    auto pView = fixture.pFactory->CreateResearchView(ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);

    CHECK_FALSE(fixture.graphics.TextYs("Build Tech").empty());
    CHECK(fixture.graphics.TextYs("build_tech").empty());
    // A real target is never reported as "None".
    CHECK(fixture.graphics.TextYs("None").empty());
}

TEST_CASE("The research panel reports no target as None", "[ui][research]")
{
    ViewFixture fixture;
    fixture.pPlayer->GetResearch().ClearResearchTarget();

    auto pView = fixture.pFactory->CreateResearchView(ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);

    CHECK_FALSE(fixture.graphics.TextYs("None").empty());
}

TEST_CASE("Editing a slot drops the selected design instead of showing both", "[ui][unit-designer]")
{
    // Selecting a design copies it into the draft. Editing a slot then changed only the draft,
    // while UnitStatusPanel kept showing the saved design's name and the list kept its
    // highlight — two different designs on screen at once.
    ViewFixture fixture;
    auto pView = fixture.pFactory->CreateUnitDesignerView(ViewFixture::FullScreen());
    REQUIRE(pView);

    FillSlot(*pView, fixture.graphics, "Chassis", "Test Chassis");
    FillSlot(*pView, fixture.graphics, "Weapon", "Test Weapon");
    FillSlot(*pView, fixture.graphics, "Armour", "Test Armor");

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    REQUIRE(ClickDrawnText(*pView, fixture.graphics, "Save"));
    REQUIRE(fixture.pPlayer->GetMilitary().GetDesigns().size() == 1);

    const std::string designName = fixture.pPlayer->GetMilitary().GetDesigns().front()->GetName();
    REQUIRE_FALSE(designName.empty());

    // Select the saved design from the list: the status panel now names it.
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    REQUIRE(ClickDrawnText(*pView, fixture.graphics, designName));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    REQUIRE(fixture.graphics.AnyTextContaining(designName));

    // Change a slot. The draft is no longer that design, so nothing should still claim it is.
    FillSlot(*pView, fixture.graphics, "Weapon", "Test Weak Weapon");

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("No design"));
}

TEST_CASE("The settings panel ignores non-left clicks", "[ui][settings]")
{
    // It toggled and saved on any button that reached it, so a right-click flipped a preference
    // and wrote user_settings.json.
    ViewFixture fixture;
    auto pView = fixture.pFactory->CreateSettingsView(ViewFixture::FullScreen());
    REQUIRE(pView);

    pView->Render(fixture.graphics);
    const bool before = fixture.settings.IsPauseAtEndOfTurn();

    // Locate the row by the text it drew, so the test does not re-derive the panel's layout.
    int rowX = -1;
    int rowY = -1;
    for (const auto& rDraw : fixture.graphics.texts)
    {
        if (rDraw.text.find("Pause at End of Turn") != std::string::npos)
        {
            rowX = static_cast<int>(rDraw.x) + 2;
            rowY = static_cast<int>(rDraw.y) + 2;
            break;
        }
    }
    REQUIRE(rowX >= 0);

    pView->HandleMouse(MouseEvent_t{MouseButton_t::Right, rowX, rowY, {}, true});
    CHECK(fixture.settings.IsPauseAtEndOfTurn() == before);

    // The same point with the left button does toggle it, so the coordinates are right.
    pView->HandleMouse(MouseEvent_t{MouseButton_t::Left, rowX, rowY, {}, true});
    CHECK(fixture.settings.IsPauseAtEndOfTurn() != before);
}

TEST_CASE("The council view refuses to run without a council", "[ui][council]")
{
    // The view exists only for an active vote. Every path returned quietly on a missing council
    // or player, leaving a Vote button that did nothing.
    ViewFixture fixture;
    REQUIRE(fixture.pState->GetPlanetaryCouncil() == nullptr);

    auto pView = fixture.pFactory->CreateCouncilVoteView(ViewFixture::FullScreen());
    REQUIRE(pView);

    // Escape resolves a pending proposal before closing; with no council that is a broken
    // session, not a no-op.
    CHECK_THROWS_AS(pView->HandleKey(KeyEvent_t{Key_t::Escape, {}}), std::runtime_error);
}
