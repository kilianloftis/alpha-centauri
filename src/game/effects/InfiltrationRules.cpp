#include "game/effects/InfiltrationRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/effects/ActiveEffect.h"

#include <variant>

namespace ac
{

namespace
{

bool IsCouncilMemberTarget_(const GameState& rState, FactionId_t candidate)
{
    const Faction* pCandidate = rState.FindFaction(candidate);
    if (!pCandidate)
    {
        return false;
    }
    const PlanetaryCouncil* pCouncil = rState.GetPlanetaryCouncil();
    if (!pCouncil)
    {
        // No council ⇒ no council members. Do not treat participatesInCouncil
        // (eligibility for council construction) as membership.
        return false;
    }
    return pCouncil->IsCouncilMember(*pCandidate);
}

bool ContinuousEffectGrantsInfiltration_(const ActiveEffect_t& rEffect,
                                         FactionId_t infiltrator,
                                         FactionId_t target,
                                         const GameState& rState)
{
    if (!std::get_if<InfiltrationEffect_t>(&rEffect.config->effect))
    {
        return false;
    }
    return FactionFilterCoversTarget(*rEffect.config, infiltrator, target, rState);
}

} // namespace

bool FactionFilterCoversTarget(const std::optional<FactionFilter_t>& rFilter,
                               bool bDefaultCoversAllOthers,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               const GameState& rState,
                               std::optional<FactionId_t> actionTarget)
{
    if (candidate == beneficiary)
    {
        return false;
    }

    if (!rFilter)
    {
        return bDefaultCoversAllOthers;
    }

    switch (rFilter->kind)
    {
        case FactionFilterKind_t::ActionTarget:
            return actionTarget.has_value() && *actionTarget == candidate;
        case FactionFilterKind_t::CouncilMembers:
            return IsCouncilMemberTarget_(rState, candidate);
        case FactionFilterKind_t::PlayerType:
        {
            const Faction* pCandidate = rState.FindFaction(candidate);
            if (!pCandidate)
            {
                return false;
            }
            const bool bIsPlayer = pCandidate->IsPlayerControlled();
            return (rFilter->playerType == PlayerType_t::Player) == bIsPlayer;
        }
    }
    return false;
}

bool FactionFilterCoversTarget(const EffectConfig_t& rConfig,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               const GameState& rState,
                               std::optional<FactionId_t> actionTarget)
{
    // Default for a continuous effect: WorldGlobal reaches every other faction; any other
    // scope needs an explicit filter.
    return FactionFilterCoversTarget(rConfig.factionFilter,
                                     rConfig.scope == EffectScope_t::WorldGlobal, beneficiary,
                                     candidate, rState, actionTarget);
}

bool HasInfiltration(const GameState& rState, FactionId_t infiltrator, FactionId_t target)
{
    if (infiltrator == target)
    {
        return false;
    }
    if (rState.GetDiplomacyLedger().HasInfiltration(infiltrator, target))
    {
        return true;
    }

    const Faction* pInfiltrator = rState.FindFaction(infiltrator);
    if (!pInfiltrator)
    {
        return false;
    }

    for (const ActiveEffect_t& rEffect : pInfiltrator->GetActiveEffects().effects)
    {
        if (ContinuousEffectGrantsInfiltration_(rEffect, infiltrator, target, rState))
        {
            return true;
        }
    }

    if (const PlanetaryCouncil* pCouncil = rState.GetPlanetaryCouncil())
    {
        for (const ActiveEffect_t& rEffect : pCouncil->CollectFactionEffects(*pInfiltrator))
        {
            if (ContinuousEffectGrantsInfiltration_(rEffect, infiltrator, target, rState))
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace ac
