#include "game/combat/MoraleRules.h"

#include "game/base/BaseManager.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/MoraleCalculator.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <cmath>

namespace ac
{

namespace
{

// Sum MoraleBonus Add contributions, optionally only conditional or only unconditional.
// rCtx must already carry the unit's subjects (see AdjustLiveBonus_): matching goes through
// StatModifierMatchesInContext so an amount_source whose subject is absent is dropped here
// rather than thrown from AmountSourceValue below.
int SumMoraleBonus_(const Unit& rUnit, const EffectContext_t& rCtx, bool bConditionalOnly)
{
    int total = 0;
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rUnit).effects)
    {
        const StatModifierEffect_t* pMod =
            std::get_if<StatModifierEffect_t>(&rEffect.config->effect);
        if (!pMod || pMod->op != ModifierOp_t::Add)
        {
            continue;
        }
        if (rEffect.config->condition.has_value() != bConditionalOnly)
        {
            continue;
        }
        if (!StatModifierMatchesInContext(rEffect, StatId_t::MoraleBonus, rCtx))
        {
            continue;
        }
        total += static_cast<int>(std::lround(AmountSourceValue(*pMod, &rCtx)));
    }
    return total;
}

bool HomeBaseHasCreche_(const Unit& rUnit)
{
    const BaseManager* pHome = rUnit.GetHomeBase();
    if (!pHome)
    {
        return false;
    }
    // Creche is a ThisBase RuleFlag on the home base's buildings.
    for (const ActiveEffect_t& rEffect : pHome->CollectBuildingEffects())
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == RuleFlagId_t::Creche)
        {
            return true;
        }
    }
    return false;
}

int AdjustLiveBonus_(const Unit& rUnit, const EffectContext_t& rCtx)
{
    // Stamp the unit's subjects once: callers build a combat/UI context that carries no
    // faction, and MoraleBonus is Unit-domain, so BasesOwned-style sources are legal on it.
    const EffectContext_t ctx = UnitSubjectContext(&rUnit, rCtx);
    int unconditional = SumMoraleBonus_(rUnit, ctx, /*bConditionalOnly=*/false);
    int conditional = SumMoraleBonus_(rUnit, ctx, /*bConditionalOnly=*/true);

    // Children's Crèche (home): soften negative SE-style morale_bonus (toward 0).
    if (HomeBaseHasCreche_(rUnit) && unconditional < 0)
    {
        unconditional /= 2; // toward 0 for negatives in C++
    }

    // SE Morale ≤ -2: PositiveMoraleScale AddPercent -50 halves Creche-style +mods.
    // Truncate toward zero so +1 * 0.5 → 0 (matches integer /2), not lround → 1.
    if (conditional > 0)
    {
        const double scale =
            ResolveMultiplicativeStat(rUnit, StatId_t::PositiveMoraleScale, 1.0, ctx);
        conditional = static_cast<int>(std::trunc(conditional * scale));
    }

    return unconditional + conditional;
}

} // namespace

int BaseIntrinsicXp(const Unit& rUnit, const MoraleCalculator& rMorale)
{
    const bool bProbeTeam = ResolveFlag(rUnit, RuleFlagId_t::ProbeTeam);
    return rMorale.IntrinsicXp(bProbeTeam);
}

int EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                         const MoraleCalculator& rMorale)
{
    const int liveBonus = AdjustLiveBonus_(rUnit, rCtx);
    return rMorale.MoraleLevel(rUnit.GetXp(), liveBonus, rCtx.combatRole);
}

std::span<const EffectConfig_t> EffectiveLevelEffects(const Unit& rUnit,
                                                      const EffectContext_t& rCtx,
                                                      const MoraleCalculator& rMorale)
{
    const int level = EffectiveMoraleLevel(rUnit, rCtx, rMorale);
    return rMorale.LevelEffects(level);
}

std::optional<CombatSide_t> PromotionCandidate(const CombatResult_t& rResult)
{
    if (rResult.bDefenderDestroyed && !rResult.bAttackerDestroyed)
    {
        return CombatSide_t::Attacker;
    }
    if (rResult.bAttackerDestroyed && !rResult.bDefenderDestroyed)
    {
        return CombatSide_t::Defender;
    }
    return std::nullopt;
}

const std::string& MoraleDisplayName(const Unit& rUnit, const MoraleCalculator& rMorale)
{
    const MoraleConfig_t& rConfig = rMorale.GetConfig();
    const int level = std::clamp(rUnit.GetXp(), rConfig.MinLevel(), rConfig.MaxLevel());
    const bool bNative = rUnit.GetDesign().IsNativeLife();
    return rMorale.DisplayName(level, bNative);
}

} // namespace ac
