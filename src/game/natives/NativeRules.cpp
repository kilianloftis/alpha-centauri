#include "game/natives/NativeRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/combat/MoraleCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/natives/NativeUnitConfig.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/natives/PlanetFaunaCalculator.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <span>
#include <stdexcept>

namespace ac
{

namespace
{

const NativeUnitPlanetConfig_t* FindUnitPlanetConfig_(const Unit& rUnit)
{
    const NativeUnitRegistry* pNatives = CurrentGameState().GetGameData().nativeUnitRegistry.get();
    if (!pNatives)
    {
        throw std::logic_error("FindUnitPlanetConfig_: no native unit registry is available");
    }
    const NativeUnitConfig_t* pConfig = pNatives->Find(rUnit.GetDesign().GetId());
    if (!pConfig || !pConfig->planet.has_value())
    {
        return nullptr;
    }
    return &*pConfig->planet;
}

} // namespace

bool IsWildNative(const Unit& rUnit)
{
    const FactionSpecies_t species = rUnit.GetFaction().GetDefinition().identity.species;
    return rUnit.GetDesign().IsNativeLife() && IsNativeLifeFaction(species);
}

int AdjacentFungusCount(const Tile& rTile, const WorldMap& rWorldMap)
{
    int count = 0;
    ForEachNeighborInChebyshevRadius(rTile, rWorldMap, 1, [&count](const Tile* pTile, int)
    {
        if (pTile->HasFeature(ImprovementIds::k_Fungus))
        {
            ++count;
        }
    });
    return count;
}

int WildNativeLifecycleLevel(const Unit& rUnit, const MoraleCalculator& rMorale)
{
    const GameState& rGame = CurrentGameState();
    const MoraleConfig_t& rMoraleConfig = rMorale.GetConfig();

    int level = 0;
    const NativeUnitPlanetConfig_t* pPlanet = FindUnitPlanetConfig_(rUnit);
    if (pPlanet && pPlanet->lifecycle == NativeLifecycleKind_t::AdjacentFungus)
    {
        level = AdjacentFungusCount(rUnit.GetTile(), rGame.GetWorldMap()) - 2;
    }
    else
    {
        const PlanetFaunaCalculator* pCalc = rGame.GetGameData().planetFaunaCalculator.get();
        if (!pCalc)
        {
            throw std::logic_error(
                "WildNativeLifecycleLevel: no planet fauna calculator is available");
        }
        const int turnFloor = pCalc->LifecycleFloor(rGame.GetTurnNumber());
        const int promotion = std::max(0, rUnit.GetXp() - rMorale.StartingIntrinsicXp());
        level = turnFloor + promotion;
        if (pPlanet)
        {
            level += pPlanet->lifecycleBonus;
        }
    }

    return std::clamp(level, rMoraleConfig.MinLevel(), rMoraleConfig.MaxLevel());
}

int PlanetPearlsBounty(const Unit& rVictim)
{
    if (!IsWildNative(rVictim))
    {
        return 0;
    }

    const MoraleCalculator& rMorale = *CurrentGameState().GetGameData().moraleCalculator;
    const MoraleConfig_t& rConfig = rMorale.GetConfig();
    const int level = WildNativeLifecycleLevel(rVictim, rMorale);
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
