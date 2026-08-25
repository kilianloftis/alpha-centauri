#include "game/units/MoraleCalculator.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ac
{

namespace
{

// Sum MoraleBonus Add contributions, optionally only conditional or only unconditional.
int SumMoraleBonus_(const Unit& rUnit, const EffectContext_t& rCtx, bool bConditionalOnly)
{
    int total = 0;
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rUnit).effects)
    {
        const StatModifierEffect_t* pMod =
            std::get_if<StatModifierEffect_t>(&rEffect.config->effect);
        if (!pMod || pMod->stat != StatId_t::MoraleBonus || pMod->op != ModifierOp_t::Add)
        {
            continue;
        }
        const bool bHasCondition = rEffect.config->condition.has_value();
        if (bConditionalOnly != bHasCondition)
        {
            continue;
        }
        if (!ConditionSatisfied(*rEffect.config, rCtx, rEffect.originBase))
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

bool HomeBaseIsRioting_(const Unit& rUnit)
{
    const BaseManager* pHome = rUnit.GetHomeBase();
    return pHome && pHome->GetPopulation().IsRioting();
}

int AdjustLiveBonus_(const Unit& rUnit, const EffectContext_t& rCtx)
{
    int unconditional = SumMoraleBonus_(rUnit, rCtx, /*bConditionalOnly=*/false);
    int conditional = SumMoraleBonus_(rUnit, rCtx, /*bConditionalOnly=*/true);

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
            ResolveMultiplicativeStat(rUnit, StatId_t::PositiveMoraleScale, 1.0, rCtx);
        conditional = static_cast<int>(std::trunc(conditional * scale));
    }

    int live = unconditional + conditional;
    if (HomeBaseIsRioting_(rUnit))
    {
        live -= 1;
    }
    return live;
}

} // namespace

MoraleCalculator::MoraleCalculator(const MoraleConfig_t& rConfig)
    : m_rConfig(rConfig)
{
}

const MoraleConfig_t& MoraleCalculator::GetConfig() const
{
    return m_rConfig;
}

int MoraleCalculator::BaseIntrinsicXp(const Unit& rUnit) const
{
    if (ResolveFlag(rUnit, RuleFlagId_t::ProbeTeam))
    {
        return m_rConfig.probeBaseIntrinsic;
    }
    return m_rConfig.baseIntrinsic;
}

int MoraleCalculator::EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx) const
{
    const int liveBonus = AdjustLiveBonus_(rUnit, rCtx);
    int level = rUnit.GetXp() + liveBonus;
    level = std::clamp(level, m_rConfig.MinLevel(), m_rConfig.MaxLevel());
    if (rCtx.combatRole == CombatRole_t::Defender)
    {
        level = std::max(level, m_rConfig.defenseFloorIndex);
    }
    return level;
}

double MoraleCalculator::CombatMoraleAddPercent(const Unit& rUnit,
                                                 const EffectContext_t& rCtx) const
{
    const int level = EffectiveMoraleLevel(rUnit, rCtx);
    const MoraleLevel_t* pLevel = m_rConfig.FindLevel(level);
    if (!pLevel)
    {
        return 0.0;
    }
    return pLevel->combatBonusPercent;
}

const std::string& MoraleCalculator::DisplayName(int level, bool bNativeLifecycle) const
{
    const MoraleLevel_t* pLevel = m_rConfig.FindLevel(level);
    static const std::string k_unknown = "?";
    if (!pLevel)
    {
        return k_unknown;
    }
    return bNativeLifecycle ? pLevel->native : pLevel->conventional;
}

const std::string& MoraleCalculator::DisplayName(const Unit& rUnit) const
{
    const bool bNative = ResolveFlag(rUnit, RuleFlagId_t::ForcesPsiCombat);
    // Display uses intrinsic rank name (not SE-shifted effective), matching stored XP.
    return DisplayName(std::clamp(rUnit.GetXp(), m_rConfig.MinLevel(), m_rConfig.MaxLevel()),
                       bNative);
}

bool MoraleCalculator::TryPromote(Unit& rSurvivor, int attackStrength, int defenseStrength,
                                  std::mt19937& rRng) const
{
    if (ResolveFlag(rSurvivor, RuleFlagId_t::ProbeTeam))
    {
        return false;
    }
    const int xp = rSurvivor.GetXp();
    if (xp >= m_rConfig.MaxLevel())
    {
        return false;
    }
    const PromotionRule_t* pRule = m_rConfig.FindPromotionRule(xp);
    if (!pRule)
    {
        throw std::runtime_error("No promotion rule found for XP " + std::to_string(xp));
    }

    double chance = 0.0;
    switch (pRule->formula)
    {
        case PromotionFormula_t::FlatChance:
            chance = pRule->chance;
            break;
        case PromotionFormula_t::DefenseOverTotal:
        {
            const int total = attackStrength + defenseStrength;
            chance = total > 0 ? static_cast<double>(defenseStrength) / total : 0.0;
            break;
        }
        case PromotionFormula_t::DefenseOverTotalHalf:
        {
            const int total = attackStrength + defenseStrength;
            chance = total > 0 ? (static_cast<double>(defenseStrength) / total) * 0.5 : 0.0;
            break;
        }
    }

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (dist(rRng) >= chance)
    {
        return false;
    }
    rSurvivor.SetXp(xp + 1);
    return true;
}

} // namespace ac
