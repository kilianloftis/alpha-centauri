#pragma once

#include "lib/effects/EffectEnums.h"

#include <optional>
#include <string>
#include <variant>

namespace ac
{

enum class ImprovementType
{
    Farm,
    Condenser
    // TODO: add more improvement types as they are defined
};

enum class EffectScope_t
{
    ThisBase,
    AllOwnerBases,
    ThisUnit,
    FactionUnits,
    FactionGlobal,
    WorldGlobal,
    // Only the specific pop instance the effect belongs to. Resolved locally by Pop
    // (e.g. ApplyTileMultipliers) and must never enter the base-wide active effects pool.
    ThisPop,
};

enum class EffectPersistence_t
{
    Instantaneous,
    Continuous,
};

enum class ModifierOp
{
    Add,
    MultiplyGeometric,
    MultiplyArithmetic
};

struct GrantBuildingEffect_t
{
    std::string buildingId;
};

struct GrantTechEffect_t
{
    std::string techId;
};

struct GrantUnitEffect_t
{
    std::string unitId;
};

struct StatModifierEffect_t
{
    StatId stat = StatId::Nutrients;
    double amount = 0.0;
    ModifierOp op = ModifierOp::Add;
};

struct RuleFlagEffect_t
{
    RuleFlagId flag;
};

struct SocialEngineeringOverrideEffect_t
{
    // TODO: define parameters when social engineering rules are finalized
    std::string category;
    std::string choice;
};

struct DiplomaticModifierEffect_t
{
    // TODO: define parameters when diplomatic modifier rules are finalized
    std::string targetFactionId;
    int value;
};

enum class TileSelectorKind
{
    BaseTile,
    HasImprovement
};

struct TileSelector_t
{
    TileSelectorKind kind;
    std::optional<ImprovementType> improvement; // set only when kind == HasImprovement
};

struct TileYieldModifierEffect_t
{
    StatId resource;
    TileSelector_t selector;
    double amount;
    ModifierOp op;
};

struct UnitBonusTableEffect_t
{
    std::string tableName;
    std::string key;
    float value = 0.0f;
};

using EffectVariant_t = std::variant<
    GrantBuildingEffect_t,
    GrantTechEffect_t,
    GrantUnitEffect_t,
    StatModifierEffect_t,
    RuleFlagEffect_t,
    SocialEngineeringOverrideEffect_t,
    DiplomaticModifierEffect_t,
    TileYieldModifierEffect_t,
    UnitBonusTableEffect_t
>;

struct EffectConfig_t
{
    EffectVariant_t effect;
    EffectScope_t scope;
    EffectPersistence_t persistence;
    std::string condition;
};

} // namespace ac
