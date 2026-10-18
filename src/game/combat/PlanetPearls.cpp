#include "game/combat/PlanetPearls.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/economy/EconomyManager.h"
#include "game/combat/MoraleCalculator.h"
#include "game/natives/NativeRules.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <span>

namespace ac
{

void GrantPlanetPearls(const MoraleCalculator& rMorale, Faction& rKiller, const Unit& rVictim)
{
    if (!IsWildNative(rVictim))
    {
        return;
    }

    const MoraleConfig_t& rConfig = rMorale.GetConfig();
    const int level = std::clamp(rVictim.GetXp(), rConfig.MinLevel(), rConfig.MaxLevel());
    const MoraleLevel_t* pLevel = rConfig.FindLevel(level);
    const std::span<const EffectConfig_t> levelEffects =
        pLevel != nullptr ? std::span<const EffectConfig_t>(pLevel->effects)
                          : std::span<const EffectConfig_t>{};

    EffectContext_t ctx;
    ctx.pUnit = &rVictim;
    const int pearls = std::max(
        0, ResolveCombatUnitStat(rVictim, StatId_t::PlanetPearls, ctx, levelEffects));
    if (pearls > 0)
    {
        rKiller.GetEconomy().AddEnergy(pearls);
    }
}

} // namespace ac
