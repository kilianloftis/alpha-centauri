#pragma once

inputs.psychAvailable = rBase.GetResources().GetPsych();#include "game/faction/base/population/CompositionInputs.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/faction/base/population/AssimilationTracker.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/population/calculators/RiotCalculator.h"
#include "game/population/calculators/GoldenAgeCalculator.h"

#include <functional>
#include <memory>
#include <string>

namespace ac
{

struct RiotConditionInputs_t;
struct GrowthConfig_t;
class PopTypeRegistry;
class PopTypeAvailabilityCalculator;
class DroneCalculator;
class PopCompositionCalculator;
struct PopCompositionResult_t;
class ResearchManager;
class BaseManager;

// PopulationManager is the API surface for the population component.
// It manages pop counts, composition, growth, and riot state for a single base,
// and provides hooks (signals) for external systems to react to population events.
class PopulationManager
{
public:
    // rBase outlives this manager — BaseManager owns it. Composition inputs are assembled from
    // the base via BuildCompositionInputs; mood thresholds come from GetCompositionConfig().
    PopulationManager(const PopTypeRegistry& rPopTypeRegistry,
                      const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
                      const GrowthConfig_t& rGrowthConfig,
                      PopCompositionCalculator& rCompositionCalculator,
                      const ResearchManager& rResearchManager,
                      BaseManager& rBase,
                      int initialSize);
    ~PopulationManager();

    // Population size management
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
    // Mood sums over the composition pool only (graph members). Not ResolveBaseStat: summing
    // the actual population avoids building virtual citizens.
    MoodWeights_t GetMoodWeightSums() const { return m_container.GetMoodWeightSums(); }

    // Bumped on every pop mutation (add/remove/convert); consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_container.GetRevision(); }

    // Add a pop of the default type (growth) or of an explicit type.
    // Throws if the base is at max size.
    void AddPop();
    void AddPop(const std::string& typeId);
    void RemovePop();

    // Convert a pop to any type by config id (e.g. "Worker", "Drone", "Talent", "Librarian").
    // Recalculates drone/talent composition when the pop changes to or from a specialist,
    // or between specialist types (psych output can change talent targets).
    void ConvertTo(Pop& rPop, const std::string& typeId);

    // Convert a pop to the registry default worker type (same composition rules as ConvertTo).
    void ConvertToDefaultPopType(Pop& rPop);

    // Convert a pop to its configured fallback type, resolved through the obsolescence chain.
    // Same composition recalculation rules as ConvertTo.
    void ConvertToFallback(Pop& rPop);

    const PopCompositionConfig_t& GetCompositionConfig() const;

    // Combat capture / probe mind-control: start or reverse the recently-conquered drone
    // window. Call before TransferBaseTo so the transfer's composition recalculation sees
    // the new peak/duration. Recapture by a faction that still has a claim (including the
    // original owner after a third party took the base) inverts that faction's elapsed time.
    void NotifyCaptured(FactionId_t previousOwner, FactionId_t newOwner);
    void AdvanceAssimilation();

    const AssimilationTracker& GetAssimilation() const { return m_assimilation; }

    // Drone and talent calculations
    bool IsRioting() const;
    bool IsDestroyed() const;

    // Run phase 1 of composition and hand the result to phase 2.
    void RecalculateComposition();

    // Phase 1's answer for this base, recomputed on demand. Exposed because the UI previews
    // what this turn's psych will do to the base before the player commits to it.
    PopCompositionResult_t ComputeComposition() const;

    // Defers specialist-driven RecalculateComposition until the outermost batch ends, so
    // multi-pop conversions (reset / auto-assign overflow) apply composition once.
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

    // Check riot conditions at end of turn. Delegates to m_riot.Update(inputs).
    void CheckRiotEndOfTurn();

    // Force an active drone riot for `turns` end-of-turn passes (probe action). Does not alter
    // pop composition, so it needs its own lifetime — the natural condition will not sustain it.
    void ForceRiot(int turns);

    // Check golden age conditions at end of turn. Delegates to m_goldenAge.Update(...).
    void CheckGoldenAgeEndOfTurn();
    bool IsInGoldenAge() const;

    // Population limits (initial value from GrowthConfig_t::maxBaseSize).
    // Hab Complex / Habitation Dome should raise this via SetMaxSize (TODO).
    int GetMaxSize() const;
    void SetMaxSize(int maxSize);

    // Nutrient stockpile owned by this manager (growth bank).
    int GetNutrientStockpile() const;
    void SetNutrientStockpile(int amount);

    // Nutrients required for the next population growth step.
    // rBaseEffects is this base's final effect list (BaseEffectsCache::Get).
    int GetNutrientsRequired(const BaseEffects_t& rBaseEffects) const;

    // Apply nutrients produced this turn: add to stockpile, grow or starve if threshold is met.
    // At max size, nutrients bank but the growth threshold is not spent.
    void ApplyGrowth(int nutrients, const BaseEffects_t& rBaseEffects);

    // Ownership transfer (BaseManager::RebindFaction): pop fallback/obsolescence resolution
    // is research-gated, so a transferred base's pops must read the new owner's discovered
    // techs from the next conversion.
    void RebindResearch(const ResearchManager& rResearch);

    // Signals
    Signal<int> OnPopGained;   // new size
    Signal<int> OnPopLost;     // new size
    // Emitted with the pop about to be removed, while it is still valid, so an observer can
    // drop its reference — the guarantee UnitManager::OnUnitDestroyed already provides.
    // OnPopLost follows with the new size once the pop is gone.
    Signal<Pop&> OnPopRemoved;

    // How much a pop is currently worth, for deciding which one is lost when the base shrinks.
    // Injected because the answer needs tile yields, which only BaseManager can resolve — the
    // same shape as WorkerAssignmentManager::SetTileScorer. Unset means every pop scores equal.
    void SetPopValuator(std::function<int(const Pop&)> valuator);

    // Riot signals
    Signal<> OnWillRiot;    // conditions met after growth, riot not yet active
    Signal<> OnIsRioting;   // end-of-turn: conditions still met, riot now active
    Signal<> OnRiotEnded;   // end-of-turn: conditions no longer met, riot was active

    // Growth signals
    Signal<> OnGrowth;       // base has gained a pop
    Signal<> OnStarvation;   // base has lost a pop

    // Golden age signals
    Signal<> OnGoldenAgeStarted;
    Signal<> OnGoldenAgeEnded;

private:
    PopContainer m_container;
    const PopTypeRegistry& m_rRegistry;
    // The rules services the container used to hold. They live here because this class owns
    // population policy — conversion legality, the obsolescence chain, composition targets.
    const PopTypeAvailabilityCalculator& m_rAvailabilityCalculator;
    // A pointer only because RebindResearch re-points it when the base changes owner.
    const ResearchManager* m_pResearch;
    const GrowthConfig_t& m_rGrowthConfig;
    PopCompositionCalculator& m_rCompositionCalculator;
    BaseManager& m_rBase;
    int m_maxSize;
    int m_nutrientStockpile = 0;
    int m_compositionBatchDepth = 0;
    bool m_bCompositionDirty = false;
    std::function<int(const Pop&)> m_popValuator;

    RiotCalculator m_riot;
    GoldenAgeCalculator m_goldenAge;
    AssimilationTracker m_assimilation;

    RiotConditionInputs_t BuildRiotInputs_() const;
    CompositionEffectInputs_t BuildCompositionInputs_() const;

    void NotifyPopGained_();
    void NotifyPopLost_();
    void MaybeRecalculateComposition_();
    // Specialists / player-choice pops last; within a group, the pop producing the least. See
    // docs/game-rules-decisions.md, "Which pop is lost when a base shrinks".
    Pop& SelectDoomedPop_();
    const std::string& GetDefaultPopType_() const;
    // Requested type id -> the type a pop actually becomes, walking the obsolescence chain
    // against currently discovered techs. The one place that rule is applied.
    const PopTypeConfig_t& ResolveType_(const std::string& typeId) const;
    // ConvertTo without the specialist-change recalculation hook, for callers that are already
    // inside a recalculation.
    void ConvertResolved_(Pop& rPop, const std::string& typeId);
};

} // namespace ac
