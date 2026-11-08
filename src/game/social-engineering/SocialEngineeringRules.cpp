#include "game/social-engineering/SocialEngineeringRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/economy/EconomyManager.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyDraft.h"
#include "game/social-engineering/UpheavalCostCalculator.h"

#include <magic_enum.hpp>
#include <variant>

namespace ac
{

namespace
{

bool PolicyForbidden_(const Faction& rFaction, const SocialPolicyId_t& rPolicyId)
{
    for (const ActiveEffect_t& rEffect : rFaction.GetActiveEffects().effects)
    {
        const auto* pForbidden =
            std::get_if<ForbiddenSocialPolicyEffect_t>(&rEffect.config->effect);
        if (pForbidden && pForbidden->policy == rPolicyId)
        {
            return true;
        }
    }
    return false;
}

int CountChangedCategories_(const Faction& rFaction, const SocialPolicyDraft_t& rDraft)
{
    const SocialEngineeringManager& rSe = rFaction.GetSocialEngineering();
    int changed = 0;
    for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
    {
        const SocialPolicyConfig_t& rProposed = rDraft.Get(category);
        const SocialPolicyConfig_t& rCommitted = rSe.GetCommittedPolicy(category);
        if (rProposed.id != rCommitted.id)
        {
            ++changed;
        }
    }
    return changed;
}

int ResolveUpheavalCost_(const Faction& rFaction)
{
    const double seed = SeedFor(StatId_t::UpheavalCost);
    const double resolved =
        ResolveFactionStat(rFaction.GetActiveEffects(), StatId_t::UpheavalCost, seed, nullptr);
    return FinalizeResolvedStat(resolved);
}

// SMAC applies cost += cost/2 with trunc toward 0; lround would round .5 away from zero.
int ApplyUpheavalCostScale_(const Faction& rFaction, int formulaTimesCost)
{
    const double seed = static_cast<double>(formulaTimesCost);
    const double scaled = ResolveFactionStat(
        rFaction.GetActiveEffects(), StatId_t::UpheavalCostScale, seed, nullptr);
    return static_cast<int>(scaled);
}

} // namespace

bool PolicyAvailable(const Faction& rFaction, const SocialPolicyConfig_t& rPolicy)
{
    if (!rPolicy.IsAvailable(rFaction.GetResearch().GetDiscoveredTechs()))
    {
        return false;
    }
    return !PolicyForbidden_(rFaction, rPolicy.id);
}

int QuoteUpheavalAbsolute(const Faction& rFaction, const SocialPolicyDraft_t& rDraft)
{
    UpheavalCostInputs_t inputs;
    inputs.changedCategories = CountChangedCategories_(rFaction, rDraft);

    const UpheavalCostCalculator& rCalculator =
        *CurrentGameState().GetGameData().upheavalCostCalculator;
    const int cubed = rCalculator.Quote(inputs);
    const int baseCost = ResolveUpheavalCost_(rFaction);
    return ApplyUpheavalCostScale_(rFaction, cubed * baseCost);
}

int QuoteUpheavalDelta(const Faction& rFaction, const SocialPolicyDraft_t& rDraft)
{
    const int absolute = QuoteUpheavalAbsolute(rFaction, rDraft);
    return absolute - rFaction.GetSocialEngineering().GetUpheavalPaid();
}

bool CanAdoptDraft(const Faction& rFaction, const SocialPolicyDraft_t& rDraft)
{
    for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
    {
        if (!PolicyAvailable(rFaction, rDraft.Get(category)))
        {
            return false;
        }
    }
    const int delta = QuoteUpheavalDelta(rFaction, rDraft);
    if (delta <= 0)
    {
        return true;
    }
    return rFaction.GetEconomy().CanAfford(delta);
}

} // namespace ac
