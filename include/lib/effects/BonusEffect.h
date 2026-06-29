#pragma once

#include "lib/effects/StatId.h"

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
    FactionUnits,
    FactionGlobal,
    WorldGlobal,
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
    std::string flag;
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

using EffectVariant_t = std::variant<
    GrantBuildingEffect_t,
    GrantTechEffect_t,
    GrantUnitEffect_t,
    StatModifierEffect_t,
    RuleFlagEffect_t,
    SocialEngineeringOverrideEffect_t,
    DiplomaticModifierEffect_t,
    TileYieldModifierEffect_t
>;

struct EffectConfig_t
{
    EffectVariant_t effect;
    EffectScope_t scope;
    EffectPersistence_t persistence;
    std::string condition;
};

} // namespace ac
