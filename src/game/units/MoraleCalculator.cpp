#include "game/units/MoraleCalculator.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/units/Unit.h"
#include "lib/LuaRuntime.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

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

double ResolvePromotionChanceFromLevel(double seed,
                                       std::span<const EffectConfig_t> levelEffects)
{
    std::vector<std::pair<double, ModifierOp_t>> stack;
    for (const EffectConfig_t& config : levelEffects)
    {
        const StatModifierEffect_t* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        if (!pMod || pMod->stat != StatId_t::PromotionChance)
        {
            continue;
        }
        stack.emplace_back(pMod->amount, pMod->op);
    }
    return ApplyModifierStack(seed, stack);
}

MoraleCalculator::MoraleCalculator(const MoraleConfig_t& rConfig, LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
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

std::span<const EffectConfig_t> MoraleCalculator::EffectiveLevelEffects(
    const Unit& rUnit, const EffectContext_t& rCtx) const
{
    const MoraleLevel_t* pLevel = m_rConfig.FindLevel(EffectiveMoraleLevel(rUnit, rCtx));
    if (!pLevel)
    {
        return {};
    }
    return pLevel->effects;
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
    const MoraleLevel_t* pLevel = m_rConfig.FindLevel(xp);
    if (!pLevel)
    {
        throw std::runtime_error("No morale level found for XP " + std::to_string(xp));
    }

    const std::unordered_map<std::string, double> vars = {
        {"attack_strength",  static_cast<double>(attackStrength)},
        {"defense_strength", static_cast<double>(defenseStrength)},
    };
    const double seed =
        m_rLua.EvalDouble(m_rConfig.promotionSeedFormula, vars);
    const double chance = ResolvePromotionChanceFromLevel(seed, pLevel->effects);

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (dist(rRng) >= chance)
    {
        return false;
    }
    rSurvivor.SetXp(xp + 1);
    return true;
}

} // namespace ac
