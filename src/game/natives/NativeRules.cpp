#include "game/natives/NativeRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/combat/MoraleCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"

#include <algorithm>
#include <span>

namespace ac
{

bool IsWildNative(const Unit& rUnit)
{
    const FactionSpecies_t species = rUnit.GetFaction().GetDefinition().identity.species;
    return rUnit.GetDesign().IsNativeLife() && IsNativeLifeFaction(species);
}

int PlanetPearlsBounty(const Unit& rVictim)
{
    if (!IsWildNative(rVictim))
    {
        return 0;
    }

    const MoraleCalculator& rMorale = *CurrentGameState().GetGameData().moraleCalculator;
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
    return pearls;
}

} // namespace ac
