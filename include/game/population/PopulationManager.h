#pragma once

#include "game/population/CompositionInputs.h"
#include "game/base/BaseTypes.h"

#include <optional>
#include "game/population/PopContainer.h"
#include "game/population/AssimilationTracker.h"
#include "game/population/PopCompositionCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/population/RiotLatch.h"
#include "game/population/GoldenAgeLatch.h"
#include "game/population/GrowthRules.h"
#include "lib/Revision.h"

#include <memory>
#include <string>

namespace ac
{

struct GrowthConfig_t;
class PopTypeRegistry;
class PopTypeAvailabilityCalculator;
class DroneCalculator;
class PopCompositionCalculator;
struct PopCompositionResult_t;
class ResearchManager;
class BaseManager;

// Pop counts, composition, growth and mood for one base, with signals for outside systems.
class PopulationManager
{
public:
    // rBase outlives this manager — BaseManager owns it. Composition inputs are assembled from
    // the base via BuildCompositionInputs; mood thresholds come from GetCompositionConfig().
    PopulationManager(const PopTypeRegistry& rPopTypeRegistry,
                      const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
                      const GrowthConfig_t& rGrowthConfig,
                      const PopCompositionCalculator& rCompositionCalculator,
                      const DroneCalculator& rDroneCalculator,
                      const ResearchManager& rResearchManager,
                      BaseManager& rBase,
                      int initialSize);
    ~PopulationManager();

    int GetSize() const;
    bool CanGrow() const;

    // Iterate pops by reference without exposing the owning unique_ptrs.
    auto Pops() { return m_container.Pops(); }
    auto Pops() const { return m_container.Pops(); }

    // Pop counts by type
    // Every tile-capable pop, including drones and talents — see PopContainer::GetWorkerCount.
    int GetWorkerCount() const { return m_container.GetWorkerCount(); }
    // Workers that are neither drones nor talents; disjoint from the two counts below.
    int GetPlainWorkerCount() const { return m_container.GetPlainWorkerCount(); }
    int GetTalentCount() const { return m_container.GetTalentCount(); }
    int GetDroneCount() const { return m_container.GetDroneCount(); }
    int GetSpecialistCount() const { return m_container.GetSpecialistCount(); }
    // Bodies available to carry drone pressure and talents (graph members only).
    int GetCompositionPoolCount() const { return m_container.GetCompositionPoolCount(); }
    // Mood sums over the composition pool only (graph members). Not ResolveBaseStat: summing
    // the actual population avoids building virtual citizens.
    MoodWeights_t GetMoodWeightSums() const { return m_container.GetMoodWeightSums(); }

    // Bumped on every pop mutation (add/remove/convert); consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_container.GetRevision(); }

    // Bumped on every mood transition, including each riot commit, since the tier changes.
    uint64_t GetMoodRevision() const { return m_moodRevision.Get(); }

    // Add a pop of the default type (growth) or of an explicit type.
    // Throws if the base is at max size.
    void AddPop();
    void AddPop(const std::string& typeId);
    void RemovePop();

    // Converts a pop by type id, forcing composition when it is in the pool or crosses the
    // specialist boundary.
    void ConvertTo(Pop& rPop, const std::string& typeId);

    // Convert a pop to the registry default worker type (same composition rules as ConvertTo).
    void ConvertToDefaultPopType(Pop& rPop);

    // Convert a pop to its configured fallback type, resolved through the obsolescence chain.
    // Same composition recalculation rules as ConvertTo.
    void ConvertToFallback(Pop& rPop);

    const PopCompositionConfig_t& GetCompositionConfig() const;

    // Starts or reverses the recently-conquered drone window; call before TransferBaseTo.
    void NotifyCaptured(FactionId_t previousOwner, FactionId_t newOwner);
    void AdvanceAssimilation();

    const AssimilationTracker& GetAssimilation() const { return m_assimilation; }

    // Mood (riot / golden age). The calculators stay private: their Forecast/Commit ordering
    // is a turn-stage contract, not something an arbitrary caller should be able to re-drive.

    // Population stage: evaluate both conditions and set pending, emitting OnWillRiot /
    // OnWillGoldenAge so the player can be warned before PlayerActions. Activates nothing.
    void ForecastMood();
    // Mood stage: commit pending state into active after the player has had a chance to act.
    void CommitMood();

    bool IsRioting() const;
    bool IsPendingRiot() const;
    // Consecutive Mood commits while rioting; selects the active riot tier. 0 when calm.
    int GetConsecutiveRiotTurns() const;
    bool IsInGoldenAge() const;

    // Probe Incite Drone Riots: force a riot for the next `turns` Mood commits.
    void ForceRiot(int turns);

    // Save / load and base transfer. Restoring emits no signals.
    MoodState_t CaptureMoodState() const;
    void RestoreMoodState(const MoodState_t& rState);
    // Ownership change: escalation is a grievance against the old owner, so the new owner
    // starts the tier ladder over rather than inheriting a rebel-tier base.
    void ResetMoodEscalation();


    // Run phase 1 of composition and reconcile actual pops (phase 2).
    void RecalculateComposition();

    // Reconcile seated pops when phase-1 inputs differ from the last apply. Safe to call
    // frequently — compares ReadCompositionInputKey and skips work when nothing moved.
    void EnsureCompositionCurrent();

    // Resets the participating pops to default workers, then seats drones and talents.
    // Specialists and player-choice types are untouched.
    void ApplyCompositionResult(const PopCompositionResult_t& rResult);

    // Phase 1's answer for this base, recomputed on demand. Exposed because the UI previews
    // what this turn's psych will do to the base before the player commits to it.
    PopCompositionResult_t ComputeComposition() const;

    // Defers composition until the outermost batch ends; Flush forces it.
    class BatchCompositionUpdate
    {
    public:
        explicit BatchCompositionUpdate(PopulationManager& rPops);
        ~BatchCompositionUpdate();
        BatchCompositionUpdate(const BatchCompositionUpdate&) = delete;
        BatchCompositionUpdate& operator=(const BatchCompositionUpdate&) = delete;

    private:
        PopulationManager& m_rPops;
    };

    // Population limits from resolved MaxBaseSize (pop_growth baselines + Hab effects).
    int GetMaxSize() const;

    // Nutrient stockpile owned by this manager (growth bank / tanks).
    int GetNutrientStockpile() const;
    void SetNutrientStockpile(int amount);

    // Total citizen eating this turn: size × intake per citizen (specialists included).
    int GetCitizenNutrientIntake() const;

    // Nutrients required for the next population growth step (tank capacity).
    // rBaseEffects is this base's final effect list (BaseEffectsCache::Get).
    int GetNutrientsRequired(const BaseEffects_t& rBaseEffects) const;

    // Applies ResolveGrowth (GrowthRules) to this turn's gross nutrient production.
    void ApplyGrowth(int nutrientProduction, const BaseEffects_t& rBaseEffects);

    // For ownership transfer: type resolution reads the new owner's techs from now on.
    void RebindResearch(const ResearchManager& rResearch);

    Signal<int> OnPopGained;   // new size
    Signal<int> OnPopLost;     // new size
    // With the pop about to be removed, while still valid; OnPopLost follows with the new size.
    Signal<const Pop&> OnPopRemoved;

    Signal<> OnWillRiot;    // Forecast: conditions met, riot not yet active
    Signal<> OnIsRioting;   // Mood Commit: riot newly active
    Signal<> OnRiotEnded;   // Mood Commit: riot ended

    Signal<> OnGrowth;       // base has gained a pop
    Signal<> OnStarvation;   // base has lost a pop

    Signal<> OnWillGoldenAge;   // Forecast: conditions met, age not yet active
    Signal<> OnGoldenAgeStarted;
    Signal<> OnGoldenAgeEnded;

private:
    PopContainer m_container;
    const PopTypeRegistry& m_rRegistry;
    // Population policy lives here: conversion legality, the obsolescence chain, composition
    // targets.
    const PopTypeAvailabilityCalculator& m_rAvailabilityCalculator;
    // A pointer only because RebindResearch re-points it when the base changes owner.
    const ResearchManager* m_pResearch;
    const GrowthConfig_t& m_rGrowthConfig;
    const PopCompositionCalculator& m_rCompositionCalculator;
    const DroneCalculator& m_rDroneCalculator;
    BaseManager& m_rBase;
    int m_nutrientStockpile = 0;
    int m_compositionBatchDepth = 0;
    bool m_bCompositionDirty = false;
    std::optional<CompositionInputKey_t> m_appliedCompositionInputKey;

    RiotLatch m_riot;
    GoldenAgeLatch m_goldenAge;
    AssimilationTracker m_assimilation;
    Revision m_moodRevision;

    GrowthOutcome_t ResolveGrowth_(int nutrientProduction, const BaseEffects_t& rBaseEffects) const;
    bool RiotConditionMet_() const;
    bool GoldenAgeConditionMet_() const;

    void MaybeRecalculateComposition_();
    // Always re-runs phase 1+2 (or marks the batch dirty). Used when seating may have diverged
    // without CompositionInputKey_t changing — ConvertTo of pool members.
    void ForceRecalculateComposition_();
    const std::string& GetDefaultPopType_() const;
    // Requested type id -> the type a pop actually becomes, walking the obsolescence chain
    // against currently discovered techs. The one place that rule is applied.
    const PopTypeConfig_t& ResolveType_(const std::string& typeId) const;
    // ConvertTo without the composition recalculation hook, for callers that are already
    // inside a recalculation.
    void ConvertResolved_(Pop& rPop, const std::string& typeId);
};

} // namespace ac
