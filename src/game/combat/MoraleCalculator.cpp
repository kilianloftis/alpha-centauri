#include "game/combat/MoraleCalculator.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "lib/LuaRuntime.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace ac
{

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

int MoraleCalculator::StartingIntrinsicXp() const
{
    return m_rConfig.startingIntrinsic;
}

int MoraleCalculator::MoraleLevel(int xp, int liveBonus) const
{
    return std::clamp(xp + liveBonus, m_rConfig.MinLevel(), m_rConfig.MaxLevel());
}

std::span<const EffectConfig_t> MoraleCalculator::LevelEffects(int level) const
{
    const MoraleLevel_t* pLevel = m_rConfig.FindLevel(level);
    if (!pLevel)
    {
        throw std::runtime_error("No morale level found for level " + std::to_string(level));
    }
    return pLevel->effects;
}

double MoraleCalculator::CombatAddPercent(int combatLevel) const
{
    const double delta = static_cast<double>(combatLevel - m_rConfig.baselineIndex);
    return delta * m_rConfig.percentPerLevel;
}

const std::string& MoraleCalculator::DisplayName(int level, MoraleLifecycle_t lifecycle) const
{
    const MoraleLevel_t* pLevel = m_rConfig.FindLevel(level);
    static const std::string k_unknown = "?";
    if (!pLevel)
    {
        return k_unknown;
    }
    if (lifecycle == MoraleLifecycle_t::Native)
    {
        return pLevel->native;
    }
    return pLevel->conventional;
}

double MoraleCalculator::PromotionChance(int xp, int attackStrength, int defenseStrength) const
{
    if (xp >= m_rConfig.MaxLevel())
    {
        return 0.0;
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
    const double seed = m_rLua.EvalDouble(m_rConfig.promotionSeedFormula, vars);
    return ResolvePromotionChanceFromLevel(seed, pLevel->effects);
}

} // namespace ac
