#include "game/units/HealingRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/map/Tile.h"
#include "game/units/HealingConfig.h"
#include "game/units/IDesign.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace ac
{

namespace
{

bool BlocksIdleHeal_(UnitTurnEvent_t event, const HealingConfig_t& rConfig)
{
    switch (event)
    {
    case UnitTurnEvent_t::MoveSpend:
        return rConfig.blockHealOnMoveSpend;
    case UnitTurnEvent_t::WasAttacker:
        return rConfig.blockHealOnWasAttacker;
    case UnitTurnEvent_t::WasDefender:
        return rConfig.blockHealOnWasDefender;
    case UnitTurnEvent_t::TookDamage:
        return rConfig.blockHealOnTookDamage;
    case UnitTurnEvent_t::Airdropped:
        return false;
    case UnitTurnEvent_t::Count:
        break;
    }
    throw std::logic_error("BlocksIdleHeal: invalid UnitTurnEvent_t");
}

std::optional<RuleFlagId_t> IgnoreHealBlockFlag_(UnitTurnEvent_t event)
{
    switch (event)
    {
    case UnitTurnEvent_t::MoveSpend:
        return RuleFlagId_t::IgnoresHealBlockOnMoveSpend;
    case UnitTurnEvent_t::WasAttacker:
        return RuleFlagId_t::IgnoresHealBlockOnWasAttacker;
    case UnitTurnEvent_t::WasDefender:
        return RuleFlagId_t::IgnoresHealBlockOnWasDefender;
    case UnitTurnEvent_t::TookDamage:
        return RuleFlagId_t::IgnoresHealBlockOnTookDamage;
    case UnitTurnEvent_t::Airdropped:
    case UnitTurnEvent_t::Count:
        return std::nullopt;
    }
    throw std::logic_error("IgnoreHealBlockFlag: invalid UnitTurnEvent_t");
}

bool IgnoresHealBlock_(const Unit& rUnit, UnitTurnEvent_t event)
{
    const std::optional<RuleFlagId_t> flag = IgnoreHealBlockFlag_(event);
    return flag.has_value() && ResolveFlag(rUnit, *flag);
}

bool IsHealIneligible_(const Unit& rUnit, const HealingConfig_t& rConfig)
{
    for (unsigned i = 0; i < static_cast<unsigned>(UnitTurnEvent_t::Count); ++i)
    {
        const UnitTurnEvent_t event = static_cast<UnitTurnEvent_t>(i);
        if (!rUnit.Latches().HasThisTurn(event) || !BlocksIdleHeal_(event, rConfig))
        {
            continue;
        }
        if (IgnoresHealBlock_(rUnit, event))
        {
            continue;
        }
        return true;
    }
    return false;
}

int FloorPercentOf_(int maxHp, int percent)
{
    const double amount =
        static_cast<double>(maxHp) * static_cast<double>(percent) / 100.0;
    return static_cast<int>(std::floor(amount));
}

struct HealStatInputs_t
{
    int maxHp = 0;
    int maxPercent = 0;
    int healPercent = 0;
};

HealStatInputs_t ResolveHealStats_(const Unit& rUnit, const HealingConfig_t& rConfig)
{
    HealStatInputs_t inputs;
    inputs.maxHp = ResolveStat(rUnit, StatId_t::HitPoints);

    const UnitEffects_t healEffects = CollectLiveUnitEffects(rUnit);
    EffectContext_t ctx;
    ctx.pUnit = &rUnit;
    ctx.targetTile = &rUnit.GetTile();

    inputs.maxPercent = static_cast<int>(std::lround(ResolveUnitStat(
        healEffects, StatId_t::MaxHealPercent, static_cast<double>(rConfig.maxHealPercent),
        &ctx)));
    inputs.healPercent = static_cast<int>(std::lround(ResolveUnitStat(
        healEffects, StatId_t::HealRate, static_cast<double>(rConfig.baseHealPercent), &ctx)));
    return inputs;
}

int MaxHealableHp_(const Unit& rUnit, const HealingConfig_t& rConfig)
{
    const HealStatInputs_t inputs = ResolveHealStats_(rUnit, rConfig);
    return FloorPercentOf_(inputs.maxHp, inputs.maxPercent);
}

int IdleHealNewHp_(const Unit& rUnit, const HealingConfig_t& rConfig)
{
    const int currentHp = rUnit.GetCurrentHp();
    const HealStatInputs_t inputs = ResolveHealStats_(rUnit, rConfig);
    if (currentHp >= inputs.maxHp || IsHealIneligible_(rUnit, rConfig))
    {
        return currentHp;
    }

    const int healed = currentHp + FloorPercentOf_(inputs.maxHp, inputs.healPercent);
    const int capped = std::min(FloorPercentOf_(inputs.maxHp, inputs.maxPercent), healed);
    return std::clamp(capped, currentHp, inputs.maxHp);
}

const HealingConfig_t& HealingConfig_()
{
    return *CurrentGameState().GetGameData().healingConfig;
}

} // namespace

bool BlocksIdleHeal(UnitTurnEvent_t event)
{
    return BlocksIdleHeal_(event, HealingConfig_());
}

bool IsHealIneligible(const Unit& rUnit)
{
    return IsHealIneligible_(rUnit, HealingConfig_());
}

int MaxHealableHp(const Unit& rUnit)
{
    return MaxHealableHp_(rUnit, HealingConfig_());
}

int IdleHealNewHp(const Unit& rUnit)
{
    return IdleHealNewHp_(rUnit, HealingConfig_());
}

} // namespace ac
