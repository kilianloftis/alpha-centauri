#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/CompositionInputs.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/faction/ResearchManager.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace ac
{

PopulationManager::PopulationManager(const PopTypeRegistry& rPopTypeRegistry,
                                     const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
                                     const GrowthConfig_t& rGrowthConfig,
                                     PopCompositionCalculator& rCompositionCalculator,
                                     const ResearchManager& rResearchManager,
                                     BaseManager& rBase,
                                     int initialSize)
    : m_container(rPopTypeRegistry, initialSize)
    , m_rRegistry(rPopTypeRegistry)
    , m_rAvailabilityCalculator(rPopTypeAvailabilityCalculator)
    , m_pResearch(&rResearchManager)
    , m_rGrowthConfig(rGrowthConfig)
    , m_rCompositionCalculator(rCompositionCalculator)
    , m_rBase(rBase)
    // The cap comes from pop_growth.json; there is no second, compiled-in default to drift.
    , m_maxSize(rGrowthConfig.maxBaseSize)
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
    return m_container.GetSize() < m_maxSize;
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
    NotifyPopGained_();
    m_riot.NotifyPopGrown(BuildRiotInputs_());
}

void PopulationManager::SetPopValuator(std::function<int(const Pop&)> valuator)
{
    m_popValuator = std::move(valuator);
}

Pop& PopulationManager::SelectDoomedPop_()
{
    // Pops the player deliberately chose are taken last, so the comparison key leads with
    // "is a player-choice type" — false sorts first. Within a group the pop producing the least
    // total resource goes; ties keep the earliest, which makes the choice deterministic rather
    // than allocation-order dependent. Default workers are assignable but not player-choice:
    // they are what composition converts from.
    Pop* pDoomed = nullptr;
    std::pair<bool, int> worst;
    for (Pop& rPop : m_container.Pops())
    {
        const std::pair<bool, int> key{rPop.IsPlayerChoiceType(),
                                       m_popValuator ? m_popValuator(rPop) : 0};
        if (!pDoomed || key < worst)
        {
            pDoomed = &rPop;
            worst = key;
        }
    }
    if (!pDoomed)
    {
        throw std::runtime_error("PopulationManager::RemovePop: base has no population");
    }
    return *pDoomed;
}

void PopulationManager::RemovePop()
{
    Pop& rDoomed = SelectDoomedPop_();
    OnPopRemoved.Emit(rDoomed);
    m_container.Remove(rDoomed);

    // Targets are a function of base size, so a removal invalidates them immediately. Conquest,
    // a probe pop-kill and starvation all shrink a base mid-turn and used to leave the
    // drone/talent split describing the size the base no longer has until the next Population
    // stage. Not done on AddPop: the caller there names the type it wants, and reconciling
    // inside the add would silently overwrite it.
    //
    // Before OnPopLost, not after: observers of that signal — including EventBridge's
    // EvBaseLostPop, which is mod-facing — read the composition, and announcing the new size
    // alongside the old split just moves the staleness into them.
    MaybeRecalculateComposition_();
    NotifyPopLost_();
}

const PopTypeConfig_t& PopulationManager::ResolveType_(const std::string& typeId) const
{
    // The single place a requested pop type becomes an actual one. Every conversion path goes
    // through the obsolescence chain, so a pop always lands on the most current non-obsoleted
    // successor of what was asked for. Previously only the fallback path did this, and plain
    // ConvertTo installed the raw id — so it could seat a type the fallback path would refuse.
    return m_rAvailabilityCalculator.ResolveCurrentType(typeId,
                                                        m_pResearch->GetDiscoveredTechs());
}

void PopulationManager::ConvertResolved_(Pop& rPop, const std::string& typeId)
{
    m_container.ConvertTo(rPop, ResolveType_(typeId));
}

void PopulationManager::ConvertTo(Pop& rPop, const std::string& typeId)
{
    // A conversion to or from a non-tile-worker changes this base's psych output, which is a
    // composition input — so the split has to be recomputed against the new psych.
    const bool bWasSpecialist = !rPop.IsWorker();
    ConvertResolved_(rPop, typeId);
    if (bWasSpecialist || !rPop.IsWorker())
    {
        MaybeRecalculateComposition_();
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
    // Destructors are implicitly noexcept, and the deferred work here can throw: it reaches
    // GetDefaultPopType_ (registry) and ResolveType_ (obsolescence chain), both of which throw
    // on a config the registry cannot satisfy. Letting that escape calls std::terminate — and
    // this guard sits on the hot worker-assignment paths, so it would take the process down
    // rather than surface a config error. Report and swallow instead; the composition is left
    // stale, which is recoverable, unlike termination.
    try
    {
        m_rPops.EnsureCompositionCurrent();
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
    return m_maxSize;
}

void PopulationManager::SetMaxSize(int maxSize)
{
    // TODO: Hab Complex / Habitation Dome buildings should call this to raise the
    // population cap (SMAC: 7 without Hab Complex, 14 without Hab Dome).
    m_maxSize = maxSize;
    // Trim excess pops if max size decreased
    while (m_container.GetSize() > m_maxSize)
    {
        RemovePop();
    }
}

int PopulationManager::GetNutrientStockpile() const
{
    return m_nutrientStockpile;
}

void PopulationManager::SetNutrientStockpile(int amount)
{
    m_nutrientStockpile = amount;
}

int PopulationManager::GetNutrientsRequired(const BaseEffects_t& rBaseEffects) const
{
    return GrowthCalculator::ComputeNutrientsRequired(m_rGrowthConfig, GetSize(), rBaseEffects);
}

void PopulationManager::ApplyGrowth(int nutrients, const BaseEffects_t& rBaseEffects)
{
    m_nutrientStockpile += nutrients;

    if (m_nutrientStockpile < 0)
    {
        m_nutrientStockpile = 0;
        OnStarvation.Emit();
        return;
    }

    // At the population cap, bank nutrients but do not spend the growth threshold
    // on a pop that cannot appear. Hab buildings raise the cap via SetMaxSize.
    if (!CanGrow())
    {
        return;
    }

    const int required = GrowthCalculator::ComputeNutrientsRequired(m_rGrowthConfig, GetSize(), rBaseEffects);
    if (m_nutrientStockpile >= required)
    {
        m_nutrientStockpile -= required;
        OnGrowth.Emit();
    }
}

RiotConditionInputs_t PopulationManager::BuildRiotInputs_() const
{
    const PopCompositionConfig_t& rConfig = GetCompositionConfig();
    RiotConditionInputs_t inputs;
    inputs.riotSum = m_container.GetMoodWeightSums().riot;
    inputs.threshold = rConfig.riotThreshold;
    return inputs;
}

void PopulationManager::ForecastMood()
{
    m_riot.Forecast(BuildRiotInputs_());
    m_goldenAge.Forecast(BuildGoldenAgeInputs_());
    // Pending state carries no effects, but the UI reads it; no revision bump.
}

void PopulationManager::CommitMood()
{
    m_riot.Commit(BuildRiotInputs_());
    m_goldenAge.Commit(BuildGoldenAgeInputs_());
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

bool PopulationManager::IsPendingGoldenAge() const
{
    return m_goldenAge.IsPending();
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

bool PopulationManager::IsDestroyed() const
{
    return m_container.GetSize() == 0;
}

CompositionEffectInputs_t PopulationManager::BuildCompositionInputs_() const
{
    return BuildCompositionInputs(m_rBase);
}

PopCompositionResult_t PopulationManager::ComputeComposition() const
{
    const CompositionEffectInputs_t effects = BuildCompositionInputs_();

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
    m_appliedCompositionInputKey = ReadCompositionInputKey(m_rBase);
}

void PopulationManager::EnsureCompositionCurrent()
{
    if (m_compositionBatchDepth > 0)
    {
        return;
    }
    const CompositionInputKey_t current = ReadCompositionInputKey(m_rBase);
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

GoldenAgeCalculator::Inputs_t PopulationManager::BuildGoldenAgeInputs_() const
{
    GoldenAgeCalculator::Inputs_t inputs;
    inputs.droneCount = m_container.GetDroneCount();
    inputs.goldenAgeSum = m_container.GetMoodWeightSums().goldenAge;
    inputs.threshold = GetCompositionConfig().goldenAgeThreshold;
    return inputs;
}

void PopulationManager::NotifyPopGained_()
{
    OnPopGained.Emit(GetSize());
}

void PopulationManager::NotifyPopLost_()
{
    OnPopLost.Emit(GetSize());
}

} // namespace ac
