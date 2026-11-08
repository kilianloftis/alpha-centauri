#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/economy/EconomyManager.h"
#include "game/effects/EffectConfig.h"
#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/social-engineering/SocialEngineeringMutations.h"
#include "game/social-engineering/SocialEngineeringRules.h"
#include "game/social-engineering/SocialPoliciesConfig.h"
#include "game/social-engineering/SocialPolicyConfigParser.h"
#include "game/social-engineering/SocialPolicyDraft.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/UpheavalCostCalculator.h"
#include "game/social-engineering/UpheavalCostConfig.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

EffectConfig_t AlienUpheavalScaleEffect_()
{
    EffectConfig_t effect;
    effect.scope = EffectScope_t::FactionGlobal;
    StatModifierEffect_t modifier;
    modifier.stat = StatId_t::UpheavalCostScale;
    modifier.amount = 50.0;
    modifier.op = ModifierOp_t::AddPercent;
    effect.effect = modifier;
    return effect;
}

SocialPolicyDraft_t DraftReplacing_(Faction& rFaction, const SocialPolicyConfig_t& rPolicy)
{
    SocialPolicyDraft_t draft = PendingSocialDraft(rFaction);
    draft.Set(rPolicy);
    return draft;
}

} // namespace

TEST_CASE("UpheavalCostCalculator: cost_formula cubes changed categories; N=0 is free",
          "[social-engineering][upheaval]")
{
    actest::FactionFixture fixtures;
    const UpheavalCostCalculator& rCalculator = *fixtures.dataContext.upheavalCostCalculator;

    CHECK(rCalculator.Quote({.changedCategories = 0}) == 0);
    CHECK(rCalculator.Quote({.changedCategories = 1}) == 8);
    CHECK(rCalculator.Quote({.changedCategories = 2}) == 27);
    CHECK(rCalculator.Quote({.changedCategories = 3}) == 64);
    CHECK(rCalculator.Quote({.changedCategories = 4}) == 125);
}

TEST_CASE("AdoptDraft pays delta for a full selection, refunds on revert, commit sinks paid",
          "[social-engineering][upheaval]")
{
    actest::FactionFixture fixtures;
    Faction& rFaction = fixtures.MakeFaction();
    rFaction.GetEconomy().AddEnergy(100);

    const SocialPolicyConfig_t& rGrowth = fixtures.socialPolicies().Get("growth_policy");
    SocialPolicyDraft_t growthDraft = DraftReplacing_(rFaction, rGrowth);
    // Talent player upheaval_cost 2 → 8 * 2 = 16.
    CHECK(QuoteUpheavalAbsolute(rFaction, growthDraft) == 16);
    CHECK(QuoteUpheavalDelta(rFaction, growthDraft) == 16);
    CHECK(CanAdoptDraft(rFaction, growthDraft));

    AdoptDraft(rFaction, growthDraft);
    CHECK(rFaction.GetEconomy().GetEnergy() == 84);
    CHECK(rFaction.GetSocialEngineering().GetUpheavalPaid() == 16);
    CHECK(rFaction.GetSocialEngineering().GetActivePolicy(SocialCategory_t::Politics).id
          == "growth_policy");
    CHECK(rFaction.GetSocialEngineering().GetCommittedPolicy(SocialCategory_t::Politics).id
          == "frontier");

    const SocialPolicyConfig_t& rIndustry = fixtures.socialPolicies().Get("industry_policy");
    SocialPolicyDraft_t twoCatDraft = DraftReplacing_(rFaction, rIndustry);
    // Two categories → 27 * 2 = 54 absolute; delta = 54 - 16 = 38.
    CHECK(QuoteUpheavalAbsolute(rFaction, twoCatDraft) == 54);
    CHECK(QuoteUpheavalDelta(rFaction, twoCatDraft) == 38);
    AdoptDraft(rFaction, twoCatDraft);
    CHECK(rFaction.GetEconomy().GetEnergy() == 46);
    CHECK(rFaction.GetSocialEngineering().GetUpheavalPaid() == 54);

    const SocialPolicyConfig_t& rFrontier = fixtures.socialPolicies().Get("frontier");
    SocialPolicyDraft_t revertPolitics = DraftReplacing_(rFaction, rFrontier);
    CHECK(QuoteUpheavalDelta(rFaction, revertPolitics) == -38);
    AdoptDraft(rFaction, revertPolitics);
    CHECK(rFaction.GetEconomy().GetEnergy() == 84);
    CHECK(rFaction.GetSocialEngineering().GetUpheavalPaid() == 16);

    CommitSocialEngineering(rFaction);
    CHECK(rFaction.GetSocialEngineering().GetUpheavalPaid() == 0);
    CHECK(rFaction.GetSocialEngineering().GetCommittedPolicy(SocialCategory_t::Economics).id
          == "industry_policy");
    CHECK(rFaction.GetEconomy().GetEnergy() == 84);
}

TEST_CASE("AdoptDraft quotes the whole draft at once when two categories change together",
          "[social-engineering][upheaval]")
{
    actest::FactionFixture fixtures;
    Faction& rFaction = fixtures.MakeFaction();
    rFaction.GetEconomy().AddEnergy(100);

    SocialPolicyDraft_t draft = PendingSocialDraft(rFaction);
    draft.Set(fixtures.socialPolicies().Get("growth_policy"));
    draft.Set(fixtures.socialPolicies().Get("industry_policy"));

    // Two categories in one OK → 27 * 2 = 54.
    CHECK(QuoteUpheavalAbsolute(rFaction, draft) == 54);
    CHECK(QuoteUpheavalDelta(rFaction, draft) == 54);
    AdoptDraft(rFaction, draft);
    CHECK(rFaction.GetEconomy().GetEnergy() == 46);
    CHECK(rFaction.GetSocialEngineering().GetUpheavalPaid() == 54);
    CHECK(rFaction.GetSocialEngineering().GetActivePolicy(SocialCategory_t::Politics).id
          == "growth_policy");
    CHECK(rFaction.GetSocialEngineering().GetActivePolicy(SocialCategory_t::Economics).id
          == "industry_policy");
}

TEST_CASE("AdoptDraft refuses when the treasury cannot cover a positive delta",
          "[social-engineering][upheaval]")
{
    actest::FactionFixture fixtures;
    Faction& rFaction = fixtures.MakeFaction();
    SocialPolicyDraft_t draft = DraftReplacing_(rFaction, fixtures.socialPolicies().Get("growth_policy"));
    CHECK_FALSE(CanAdoptDraft(rFaction, draft));
    CHECK_THROWS(AdoptDraft(rFaction, draft));
}

TEST_CASE("AI upheaval uses difficulty UpheavalCost filtered to AI (Librarian 3)",
          "[social-engineering][upheaval]")
{
    actest::FactionFixture fixtures;
    fixtures.MakeFaction();
    Faction& rAi = fixtures.MakeFaction();
    rAi.GetEconomy().AddEnergy(100);

    SocialPolicyDraft_t draft = DraftReplacing_(rAi, fixtures.socialPolicies().Get("growth_policy"));
    // AI upheaval_cost 3 → 8 * 3 = 24.
    CHECK(QuoteUpheavalAbsolute(rAi, draft) == 24);
    AdoptDraft(rAi, draft);
    CHECK(rAi.GetEconomy().GetEnergy() == 76);
}

TEST_CASE("UpheavalCostScale AddPercent 50 applies the alien surcharge after the cube",
          "[social-engineering][upheaval]")
{
    actest::FactionFixture fixtures;
    FactionConfig_t alien = fixtures.factionDefinition;
    alien.identity.species = FactionSpecies_t::Progenitor;
    alien.effects.push_back(AlienUpheavalScaleEffect_());
    Faction& rFaction = fixtures.MakeFaction(alien);
    rFaction.GetEconomy().AddEnergy(100);

    SocialPolicyDraft_t draft =
        DraftReplacing_(rFaction, fixtures.socialPolicies().Get("growth_policy"));
    // Talent 2 → 16, then +50% trunc → 24.
    CHECK(QuoteUpheavalAbsolute(rFaction, draft) == 24);
}
