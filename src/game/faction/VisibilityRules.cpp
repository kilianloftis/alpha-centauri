#include "game/faction/VisibilityRules.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"

namespace ac
{

void ApplyRemoveShroud(Faction& rFaction)
{
    rFaction.GetExploredMap().MarkAll();
}

void ApplyRemoveFog(Faction& rFaction)
{
    rFaction.SetFogRemoved(true);
    rFaction.GetVisibleMap().SetRemoveFog(true);
    rFaction.GetVisibleMap().MarkAll();
}

void ApplyVisibilityRules(Faction& rFaction, const GameSettings* pSettings)
{
    const bool bRemoveFog =
        rFaction.IsFogRemoved()
        || ResolveFlag(rFaction, RuleFlagId_t::RemoveFog)
        || (rFaction.IsPlayerControlled() && pSettings
            && pSettings->GetVisibility().removeFog);
    rFaction.GetVisibleMap().SetRemoveFog(bRemoveFog);
    if (bRemoveFog)
    {
        rFaction.GetVisibleMap().MarkAll();
    }

    const bool bRemoveShroud =
        ResolveFlag(rFaction, RuleFlagId_t::RemoveShroud)
        || (pSettings && pSettings->GetVisibility().removeShroud);
    if (bRemoveShroud)
    {
        ApplyRemoveShroud(rFaction);
    }
}

} // namespace ac
