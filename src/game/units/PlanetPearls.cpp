#include "game/units/PlanetPearls.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/Faction.h"
#include "game/faction/EconomyManager.h"
#include "game/units/BaseConquestRules.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <span>

namespace ac
{

namespace
{

bool IsWildNative_(const Unit& rUnit)
{
    return rUnit.GetDesign().IsNativeLife()
        && IsNativeLifeFaction(rUnit.GetFaction().GetDefinition().identity.species);
}

} // namespace

void GrantPlanetPearls(const MoraleCalculator& rMorale, Faction& rKiller, const Unit& rVictim)
{
    if (!IsWildNative_(rVictim))
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
