// Views built through the real ViewFactory against a live GameState — the coverage package 15
// commit A was missing: the factory's null policy, and two view-level rules that were putting
// wrong information on screen.

#include "ViewFixture.h"

#include "game/Faction.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"

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
    CHECK_FALSE(fixture.graphics.TextYs("+1 Nutrients, +20% GrowthRate").empty());
}
