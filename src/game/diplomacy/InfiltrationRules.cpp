#include "game/diplomacy/InfiltrationRules.h"

#include "game/CurrentGameState.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/effects/ActiveEffect.h"
#include "game/diplomacy/DiplomacyLedger.h"

#include <variant>

namespace ac
{

namespace
{

bool IsCouncilMemberTarget_(FactionId_t candidate)
{
    const GameState& rState = CurrentGameState();
    const Faction* pCandidate = rState.FindFaction(candidate);
    if (!pCandidate)
    {
        return false;
    }
    return rState.GetPlanetaryCouncil().IsCouncilMember(*pCandidate);
}

bool FilterSelects_(const FactionFilter_t& rFilter, FactionId_t candidate,
                    std::optional<FactionId_t> actionTarget)
{
    switch (rFilter.kind)
    {
        case FactionFilterKind_t::ActionTarget:
            return actionTarget.has_value() && *actionTarget == candidate;
        case FactionFilterKind_t::CouncilMembers:
            return IsCouncilMemberTarget_(candidate);
        case FactionFilterKind_t::PlayerType:
        {
            const Faction* pCandidate = CurrentGameState().FindFaction(candidate);
            if (!pCandidate)
            {
                return false;
            }
            const bool bIsPlayer = pCandidate->IsPlayerControlled();
            return (rFilter.playerType == PlayerType_t::Player) == bIsPlayer;
        }
    }
    return false;
}

bool ContinuousEffectGrantsInfiltration_(const ActiveEffect_t& rEffect,
                                         FactionId_t infiltrator,
                                         FactionId_t target)
{
    if (!std::get_if<InfiltrationEffect_t>(&rEffect.config->effect))
    {
        return false;
    }
    return FactionFilterCoversTarget(*rEffect.config, infiltrator, target, std::nullopt);
}

} // namespace

bool FactionFilterCoversTarget(const std::optional<FactionFilter_t>& rFilter,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               std::optional<FactionId_t> actionTarget)
{
    if (candidate == beneficiary)
    {
        return false;
    }
    return !rFilter || FilterSelects_(*rFilter, candidate, actionTarget);
}

bool FactionFilterCoversTarget(const EffectConfig_t& rConfig,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               std::optional<FactionId_t> actionTarget)
{
    if (candidate == beneficiary)
    {
        return false;
    }
    // Without a filter, only WorldGlobal reaches every other faction.
    if (!rConfig.factionFilter)
    {
        return rConfig.scope == EffectScope_t::WorldGlobal;
    }
    return FilterSelects_(*rConfig.factionFilter, candidate, actionTarget);
}

namespace
{

bool AnyGrantsInfiltration_(const std::vector<ActiveEffect_t>& rEffects, FactionId_t infiltrator,
                            FactionId_t target)
{
    return std::ranges::any_of(rEffects, [&](const ActiveEffect_t& rEffect) {
        return ContinuousEffectGrantsInfiltration_(rEffect, infiltrator, target);
    });
}

} // namespace

bool HasInfiltration(FactionId_t infiltrator, FactionId_t target)
{
    const GameState& rState = CurrentGameState();
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

    if (AnyGrantsInfiltration_(pInfiltrator->GetActiveEffects().effects, infiltrator, target))
    {
        return true;
    }
    const PlanetaryCouncil& rCouncil = rState.GetPlanetaryCouncil();
    return AnyGrantsInfiltration_(rCouncil.CollectFactionEffects(*pInfiltrator), infiltrator,
                                  target);
}

} // namespace ac
