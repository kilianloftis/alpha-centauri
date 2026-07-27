#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ac
{

enum class PromotionFormula_t
{
    FlatChance,
    DefenseOverTotal,
    DefenseOverTotalHalf,
};

struct MoraleLevel_t
{
    int index = 0;
    std::string conventional;
    std::string native;
    double combatBonusPercent = 0.0;
};

struct PromotionRule_t
{
    // Inclusive range. When only `level` is set in JSON, min == max == that level.
    int minLevel = 0;
    int maxLevel = 0;
    PromotionFormula_t formula = PromotionFormula_t::FlatChance;
    // Used when formula == FlatChance (0..1).
    double chance = 0.0;
};

struct MoraleConfig_t
{
    int baseIntrinsic = 1;
    int probeBaseIntrinsic = 2;
    int defenseFloorIndex = 1;
    std::vector<MoraleLevel_t> levels;
    std::vector<PromotionRule_t> promotionRules;

    int MinLevel() const;
    int MaxLevel() const;
    const MoraleLevel_t* FindLevel(int index) const;
    const PromotionRule_t* FindPromotionRule(int intrinsicXp) const;
};

} // namespace ac
