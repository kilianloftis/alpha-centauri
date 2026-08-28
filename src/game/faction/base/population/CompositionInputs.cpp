#include "game/faction/base/population/CompositionInputs.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/AwayFromHomeDrones.h"
#include "game/faction/base/population/GarrisonPolice.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/map/WorldMap.h"
#include "game/population/calculators/DroneCalculator.h"

#include <stdexcept>

namespace ac
{

CompositionEffectInputs_t BuildCompositionInputs(const BaseManager& rBase)
{
    const GameDataContext& rData = rBase.GetFaction().GetDataContext();
    if (!rData.droneCalculator)
    {
        throw std::runtime_error("BuildCompositionInputs: GameDataContext.droneCalculator is null");
    }
    const DroneCalculator& rDrones = *rData.droneCalculator;

    // GetBaseEffects includes SE rating expansion (Efficiency -> Bureaucracy MultiplyGeometric).
    const BaseEffects_t& rBaseEffects = rBase.GetBaseEffects();
    const int baseSize = rBase.GetPopulation().GetSize();

    BureaucracyDroneInputs_t bureaucracy;
    bureaucracy.baseId = rBase.GetBaseId();
    bureaucracy.factionBaseCount = static_cast<int>(rBase.GetFaction().GetBaseCount());
    const WorldMap& rMap = rBase.GetTileEffects().GetWorldMap();
    bureaucracy.mapWidth = rMap.GetWidth();
    bureaucracy.mapHeight = rMap.GetHeight();
    bureaucracy.bureaucracy =
        ResolveBaseStat(rBaseEffects, StatId_t::Bureaucracy, SeedFor(StatId_t::Bureaucracy));

    SizeDroneInputs_t size;
    size.baseSize = baseSize;
    size.sizeFreeDrones = FinalizeResolvedStat(ResolveBaseStat(
        rBaseEffects, StatId_t::SizeFreeDrones, SeedFor(StatId_t::SizeFreeDrones)));

    OccupationDroneInputs_t occupation;
    occupation.baseSize = baseSize;
    const AssimilationState& rWindow = rBase.GetPopulation().GetAssimilation().OccupierWindow();
    occupation.turnsSinceConquered = rWindow.turnsElapsed;
    occupation.assimilationDuration = rWindow.durationTurns;
    occupation.assimilationPeak = rWindow.peakDrones;
    occupation.conqueredDroneCap = ResolveBaseStat(
        rBaseEffects, StatId_t::ConqueredDroneCap, SeedFor(StatId_t::ConqueredDroneCap));

    // The runtime terms seed the stat; facilities and SE Add on top, and pop_composition.json's
    // MinClamp 0 / MaxClamp BaseSize bound the result, through the ordinary effect pipeline.
    const double seed = rDrones.CalculateBureaucracyDrones(bureaucracy)
                      + rDrones.CalculateSizeDrones(size)
                      + rDrones.CalculateOccupationDrones(occupation)
                      + ComputeAwayFromHomeDrones(rBase)
                      - ComputeGarrisonPoliceSuppression(rBase);

    CompositionEffectInputs_t inputs;
    inputs.dronePressure =
        FinalizeResolvedStat(ResolveBaseStat(rBaseEffects, StatId_t::Drones, seed));
    inputs.resolvedTalents = FinalizeResolvedStat(
        ResolveBaseStat(rBaseEffects, StatId_t::Talents, SeedFor(StatId_t::Talents)));
    inputs.psychAvailable = rBase.GetResources().GetPsych();
    return inputs;
}

} // namespace ac
