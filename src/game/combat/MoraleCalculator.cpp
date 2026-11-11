#include "game/combat/MoraleCalculator.h"

#include "lib/LuaRuntime.h"

#include <algorithm>
#include <unordered_map>

namespace ac
{

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

double MoraleCalculator::PromotionEligibilitySeed(int ourStrength, int theirStrength) const
{
    const std::unordered_map<std::string, double> vars = {
        {"our_strength", static_cast<double>(ourStrength)},
        {"their_strength", static_cast<double>(theirStrength)},
    };
    return m_rLua.EvalDouble(m_rConfig.promotionEligibilityFormula, vars);
}

} // namespace ac
