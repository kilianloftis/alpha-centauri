#include "game/population/PopulationManager.h"
#include "game/population/CompositionInputs.h"
#include "game/base/BaseManager.h"
#include "game/population/GrowthRules.h"
#include "game/population/PopulationRules.h"
#include "game/population/PopCompositionCalculator.h"
#include "game/population/PopTypeAvailabilityCalculator.h"
#include "game/population/PopTypeConfigParser.h"
#include "game/population/PopTypeRegistry.h"
#include "game/population/GrowthConfigParser.h"
#include "game/research/ResearchManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace ac
{

PopulationManager::PopulationManager(const PopTypeRegistry& rPopTypeRegistry,
                                     const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
                                     const GrowthConfig_t& rGrowthConfig,
                                     const PopCompositionCalculator& rCompositionCalculator,
                                     const DroneCalculator& rDroneCalculator,
                                     const ResearchManager& rResearchManager,
                                     BaseManager& rBase,
                                     int initialSize)
    : m_container(rPopTypeRegistry, initialSize)
    , m_rRegistry(rPopTypeRegistry)
    , m_rAvailabilityCalculator(rPopTypeAvailabilityCalculator)
    , m_pResearch(&rResearchManager)
    , m_rGrowthConfig(rGrowthConfig)
    , m_rCompositionCalculator(rCompositionCalculator)
    , m_rDroneCalculator(rDroneCalculator)
    , m_rBase(rBase)
    , m_nutrientStockpile(0)
    , m_riot(OnWillRiot, OnIsRioting, OnRiotEnded)
    , m_goldenAge(OnWillGoldenAge, OnGoldenAgeStarted, OnGoldenAgeEnded)
{
}

PopulationManager::~PopulationManager()
{
}

void PopulationManager::RebindResearch(const ResearchManager& rResearch)
{
    m_pResearch = &rResearch;
}

int PopulationManager::GetSize() const
{
    return m_container.GetSize();
}

const std::string& PopulationManager::GetDefaultPopType_() const
{
    return m_rRegistry.GetDefault().id;
}

bool PopulationManager::CanGrow() const
{
    return m_container.GetSize() < GetMaxSize();
}

void PopulationManager::AddPop()
{
    AddPop(GetDefaultPopType_());
}

void PopulationManager::AddPop(const std::string& typeId)
{
    if (!CanGrow())
    {
        throw std::runtime_error("Cannot add pop: base is at max size");
    }
    // Resolved like any conversion: creating a pop of an obsoleted type would seat something
    // no conversion path can produce, which is the asymmetry this package removed.
    m_container.AddPop(ResolveType_(typeId));
    OnPopGained.Emit(GetSize());
    m_riot.NotifyPopGrown(RiotConditionMet_());
}

void PopulationManager::RemovePop()
{
    const Pop& rDoomed = SelectPopToRemove(m_rBase);
    OnPopRemoved.Emit(rDoomed);
    m_container.Remove(rDoomed);

    // Targets depend on base size, so a removal recalculates at once (AddPop does not, since its
    // caller names the type). Before OnPopLost, whose observers read the composition.
    MaybeRecalculateComposition_();
    OnPopLost.Emit(GetSize());
}

const PopTypeConfig_t& PopulationManager::ResolveType_(const std::string& typeId) const
{
    // The one place a requested type becomes an actual one: the most current non-obsoleted
    // successor.
    return m_rAvailabilityCalculator.ResolveCurrentType(typeId,
                                                        m_pResearch->GetDiscoveredTechs());
}

void PopulationManager::ConvertResolved_(Pop& rPop, const std::string& typeId)
{
    m_container.ConvertTo(rPop, ResolveType_(typeId));
}

void PopulationManager::ConvertTo(Pop& rPop, const std::string& typeId)
{
    // Drone ↔ Worker can leave the phase-1 inputs equal while seating is wrong, and the
    // specialist boundary changes psych, so both force composition.
    const bool bWasParticipating = rPop.ParticipatesInComposition();
    const bool bWasSpecialist = !rPop.IsWorker();
    ConvertResolved_(rPop, typeId);
    if (bWasParticipating || rPop.ParticipatesInComposition() || bWasSpecialist
        || !rPop.IsWorker())
    {
        ForceRecalculateComposition_();
    }
}

void PopulationManager::ConvertToDefaultPopType(Pop& rPop)
{
    ConvertTo(rPop, GetDefaultPopType_());
}

void PopulationManager::ConvertToFallback(Pop& rPop)
{
    const PopTypeConfig_t* pCurrent = m_rRegistry.Find(rPop.GetPopType());
    if (!pCurrent)
    {
        throw std::runtime_error("PopulationManager::ConvertToFallback: current pop type not in "
                                 "registry: " + std::string(rPop.GetPopType()));
    }
    if (pCurrent->fallbackPopTypeId.empty())
    {
        throw std::runtime_error("PopulationManager::ConvertToFallback: pop type '"
                                 + pCurrent->id + "' has no fallback configured");
    }
    // Same resolution as any other conversion — ConvertTo owns the chain walk.
    ConvertTo(rPop, pCurrent->fallbackPopTypeId);
}

PopulationManager::BatchCompositionUpdate::BatchCompositionUpdate(PopulationManager& rPops)
    : m_rPops(rPops)
{
    ++m_rPops.m_compositionBatchDepth;
}

PopulationManager::BatchCompositionUpdate::~BatchCompositionUpdate()
{
    if (--m_rPops.m_compositionBatchDepth != 0 || !m_rPops.m_bCompositionDirty)
    {
        return;
    }
    m_rPops.m_bCompositionDirty = false;
    // The deferred work can throw on a bad config, and this runs in a noexcept destructor on hot
    // paths, so it reports and leaves composition stale rather than terminate.
    try
    {
        // Force: dirty may mean ConvertTo mutated seating under an unchanged input key.
        m_rPops.RecalculateComposition();
    }
    catch (const std::exception& rError)
    {
        std::cerr << "BatchCompositionUpdate: deferred composition recalculation failed: "
                  << rError.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "BatchCompositionUpdate: deferred composition recalculation failed\n";
    }
}

int PopulationManager::GetMaxSize() const
{
    const double total = ResolveBaseStat(m_rBase.GetBaseEffects(), StatId_t::MaxBaseSize,
                                         SeedFor(StatId_t::MaxBaseSize), nullptr);
    return FinalizeResolvedStat(total);
}

int PopulationManager::GetNutrientStockpile() const
{
    return m_nutrientStockpile;
}

void PopulationManager::SetNutrientStockpile(int amount)
{
    m_nutrientStockpile = amount;
}

int PopulationManager::GetCitizenNutrientIntake() const
{
    return GetSize() * m_rGrowthConfig.nutrientIntakePerCitizen;
}

int PopulationManager::GetNutrientsRequired(const BaseEffects_t& rBaseEffects) const
{
    return ComputeNutrientsRequired(m_rGrowthConfig, GetSize(), rBaseEffects);
}

GrowthOutcome_t PopulationManager::ResolveGrowth_(int nutrientProduction,
                                                  const BaseEffects_t& rBaseEffects) const
{
    const int size = GetSize();
    GrowthInputs_t inputs;
    inputs.stockpile = m_nutrientStockpile;
    inputs.netNutrients = nutrientProduction - GetCitizenNutrientIntake();
    inputs.boxNow = ComputeNutrientsRequired(m_rGrowthConfig, size, rBaseEffects);
    inputs.boxNext = ComputeNutrientsRequired(m_rGrowthConfig, size + 1, rBaseEffects);
    inputs.bRoomToGrow = CanGrow();
    return ResolveGrowth(inputs, m_rGrowthConfig);
}

void PopulationManager::ApplyGrowth(int nutrientProduction, const BaseEffects_t& rBaseEffects)
{
    const GrowthOutcome_t outcome = ResolveGrowth_(nutrientProduction, rBaseEffects);
    if (outcome.bGrows)
    {
        m_nutrientStockpile = outcome.stockpileOnGrowth;
        OnGrowth.Emit();
    }
    m_nutrientStockpile = outcome.stockpileAfter;
    if (outcome.bStarves)
    {
        OnStarvation.Emit();
    }
}

bool PopulationManager::RiotConditionMet_() const
{
    RiotConditionInputs_t inputs;
    inputs.riotSum = m_container.GetMoodWeightSums().riot;
    inputs.forcedTurnsRemaining = m_riot.GetForcedTurnsRemaining();
    return RiotConditionMet(inputs, GetCompositionConfig());
}

void PopulationManager::ForecastMood()
{
    m_riot.Forecast(RiotConditionMet_());
    m_goldenAge.Forecast(GoldenAgeConditionMet_());
    // Pending state carries no effects, but the UI reads it; no revision bump.
}

void PopulationManager::CommitMood()
{
    m_riot.Commit(RiotConditionMet_());
    m_goldenAge.Commit(GoldenAgeConditionMet_());
    m_moodRevision.Bump();
}

bool PopulationManager::IsRioting() const
{
    return m_riot.IsActive();
}

bool PopulationManager::IsPendingRiot() const
{
    return m_riot.IsPending();
}

int PopulationManager::GetConsecutiveRiotTurns() const
{
    return m_riot.GetConsecutiveTurns();
}

bool PopulationManager::IsInGoldenAge() const
{
    return m_goldenAge.IsActive();
}

void PopulationManager::ForceRiot(int turns)
{
    m_riot.ForceRiot(turns);
    m_moodRevision.Bump();
}

MoodState_t PopulationManager::CaptureMoodState() const
{
    MoodState_t state;
    state.bRioting = m_riot.IsActive();
    state.bPendingRiot = m_riot.IsPending();
    state.forcedRiotTurnsRemaining = m_riot.GetForcedTurnsRemaining();
    state.consecutiveRiotTurns = m_riot.GetConsecutiveTurns();
    state.bInGoldenAge = m_goldenAge.IsActive();
    state.bPendingGoldenAge = m_goldenAge.IsPending();
    return state;
}

void PopulationManager::RestoreMoodState(const MoodState_t& rState)
{
    m_riot.RestoreState(rState.bRioting, rState.bPendingRiot, rState.forcedRiotTurnsRemaining,
                        rState.consecutiveRiotTurns);
    m_goldenAge.RestoreState(rState.bInGoldenAge, rState.bPendingGoldenAge);
    m_moodRevision.Bump();
}

void PopulationManager::ResetMoodEscalation()
{
    m_riot.RestoreState(false, false, 0, 0);
    m_goldenAge.RestoreState(false, false);
    m_moodRevision.Bump();
}

PopCompositionResult_t PopulationManager::ComputeComposition() const
{
    const CompositionEffectInputs_t effects = BuildCompositionInputs(m_rBase, m_rDroneCalculator);

    PopCompositionInputs_t inputs;
    inputs.dronePressure = effects.dronePressure;
    inputs.resolvedTalents = effects.resolvedTalents;
    inputs.psychAvailable = effects.psychAvailable;
    inputs.poolSize = m_container.GetCompositionPoolCount();
    return m_rCompositionCalculator.Calculate(inputs);
}

void PopulationManager::RecalculateComposition()
{
    ApplyCompositionResult(ComputeComposition());
    m_appliedCompositionInputKey = ReadCompositionInputKey(m_rBase, m_rDroneCalculator);
}

void PopulationManager::EnsureCompositionCurrent()
{
    if (m_compositionBatchDepth > 0)
    {
        return;
    }
    const CompositionInputKey_t current = ReadCompositionInputKey(m_rBase, m_rDroneCalculator);
    if (m_appliedCompositionInputKey == current)
    {
        return;
    }
    RecalculateComposition();
}

void PopulationManager::ApplyCompositionResult(const PopCompositionResult_t& rResult)
{
    const PopCompositionConfig_t& rConfig = GetCompositionConfig();
    const std::string& rDefaultTypeId = GetDefaultPopType_();

    for (Pop& rPop : m_container.Pops())
    {
        if (rPop.ParticipatesInComposition())
        {
            ConvertResolved_(rPop, rDefaultTypeId);
        }
    }

    for (const DroneSeat_t& rSeat : rResult.droneSeats)
    {
        int remaining = rSeat.count;
        for (Pop& rPop : m_container.Pops())
        {
            if (remaining <= 0)
            {
                break;
            }
            if (rPop.ParticipatesInComposition() && rPop.IsPlainWorker())
            {
                ConvertResolved_(rPop, rSeat.typeId);
                --remaining;
            }
        }
        if (remaining > 0)
        {
            throw std::runtime_error(
                "ApplyCompositionResult: could not seat " + std::to_string(remaining)
                + " pop(s) of type '" + rSeat.typeId + "' — pool smaller than phase-1 result");
        }
    }

    int talentsRemaining = rResult.expectedTalents;
    for (Pop& rPop : m_container.Pops())
    {
        if (talentsRemaining <= 0)
        {
            break;
        }
        if (rPop.ParticipatesInComposition() && rPop.IsPlainWorker())
        {
            ConvertResolved_(rPop, rConfig.talentTypeId);
            --talentsRemaining;
        }
    }
    if (talentsRemaining > 0)
    {
        throw std::runtime_error(
            "ApplyCompositionResult: could not seat " + std::to_string(talentsRemaining)
            + " talent(s) — pool smaller than phase-1 result");
    }
}

void PopulationManager::MaybeRecalculateComposition_()
{
    if (m_compositionBatchDepth > 0)
    {
        m_bCompositionDirty = true;
        return;
    }
    EnsureCompositionCurrent();
}

void PopulationManager::ForceRecalculateComposition_()
{
    if (m_compositionBatchDepth > 0)
    {
        m_bCompositionDirty = true;
        return;
    }
    RecalculateComposition();
}

const PopCompositionConfig_t& PopulationManager::GetCompositionConfig() const
{
    return m_rCompositionCalculator.GetConfig();
}

void PopulationManager::NotifyCaptured(FactionId_t previousOwner, FactionId_t newOwner)
{
    const PopCompositionConfig_t& rConfig = GetCompositionConfig();
    m_assimilation.NotifyCaptured(previousOwner, newOwner, rConfig.assimilationDrones,
                                  rConfig.assimilationDecayTurns);
}

void PopulationManager::AdvanceAssimilation()
{
    m_assimilation.Advance();
}

bool PopulationManager::GoldenAgeConditionMet_() const
{
    GoldenAgeConditionInputs_t inputs;
    inputs.droneCount = m_container.GetDroneCount();
    inputs.goldenAgeSum = m_container.GetMoodWeightSums().goldenAge;
    return GoldenAgeConditionMet(inputs, GetCompositionConfig());
}

} // namespace ac
