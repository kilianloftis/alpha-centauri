#include "game/social-engineering/SocialEngineeringMutations.h"

#include "game/economy/EconomyManager.h"
#include "game/faction/Faction.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/social-engineering/SocialEngineeringRules.h"
#include "game/social-engineering/SocialPolicyDraft.h"

#include <stdexcept>

namespace ac
{

void AdoptDraft(Faction& rFaction, const SocialPolicyDraft_t& rDraft)
{
    if (!CanAdoptDraft(rFaction, rDraft))
    {
        throw std::runtime_error("AdoptDraft: draft is not adoptable");
    }

    const int delta = QuoteUpheavalDelta(rFaction, rDraft);
    EconomyManager& rEconomy = rFaction.GetEconomy();
    if (delta > 0)
    {
        rEconomy.SpendEnergy(delta);
    }
    else if (delta < 0)
    {
        rEconomy.AddEnergy(-delta);
    }

    SocialEngineeringManager& rSe = rFaction.GetSocialEngineering();
    rSe.SetUpheavalPaid(rSe.GetUpheavalPaid() + delta);
    rSe.SetPendingDraft(rDraft);
}

void CommitSocialEngineering(Faction& rFaction)
{
    rFaction.GetSocialEngineering().CommitPending();
}

} // namespace ac
