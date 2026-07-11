#pragma once

#include "game/faction/base/population/PopContainer.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/population/calculators/RiotCalculator.h"
#include "game/population/calculators/GoldenAgeCalculator.h"

#include <memory>
#include <string>

namespace ac
{

struct RiotConditionInputs;
struct GrowthConfig_t;
class PopTypeRegistry;
class PopTypeAvailabilityCalculator;
class PopCompositionCalculator;
class ResearchManager;

// PopulationManager is the API surface for the population component.
// It manages pop counts, composition, growth, and riot state for a single base,
// and provides hooks (signals) for external systems to react to population events.
class PopulationManager
{
public:
    PopulationManager(const PopTypeRegistry* pPopTypeRegistry,
                      const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator,
                      const GrowthConfig_t* pGrowthConfig,
                      PopCompositionCalculator* pCompositionCalculator,
                      const ResearchManager* pResearchManager,
                      int initialSize);
    ~PopulationManager();

    // Population size management
    int GetSize() const;
    bool CanGrow() const;

    // Iterate pops by reference without exposing the owning unique_ptrs.
    auto Pops() { return m_container.Pops(); }
    auto Pops() const { return m_container.Pops(); }

    // Pop counts by type
    int GetWorkerCount() const { return m_container.GetWorkerCount(); }
    int GetTalentCount() const { return m_container.GetTalentCount(); }
    int GetDroneCount() const { return m_container.GetDroneCount(); }
    int GetSpecialistCount() const { return m_container.GetSpecialistCount(); }

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

    // Drone and talent calculations
    bool IsRioting() const;
    bool IsDestroyed() const;

    // Reconcile actual pop composition against calculator targets.
    // Converts workers to drones/talents (or back) to match targetDrones/targetTalents.
    // No-op if no composition calculator is set.
    void RecalculateComposition();

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

    // Check golden age conditions at end of turn. Delegates to m_goldenAge.Update(...).
    void CheckGoldenAgeEndOfTurn();

    // Population limits (initial value from GrowthConfig_t::maxBaseSize).
    // Hab Complex / Habitation Dome should raise this via SetMaxSize (TODO).
    int GetMaxSize() const;
    void SetMaxSize(int maxSize);

    // Nutrient stockpile owned by this manager (growth bank).
    int GetNutrientStockpile() const;

    // Nutrients required for the next population growth step.
    // rBaseEffects is this base's final effect list (BaseManager::BuildBaseEffects_).
    int GetNutrientsRequired(const BaseEffects_t& rBaseEffects) const;

    // Apply nutrients produced this turn: add to stockpile, grow or starve if threshold is met.
    // At max size, nutrients bank but the growth threshold is not spent.
    void ApplyGrowth(int nutrients, const BaseEffects_t& rBaseEffects);

    // Signals
    Signal<int> OnPopGained;   // new size
    Signal<int> OnPopLost;     // new size

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
    const PopTypeRegistry* m_pRegistry = nullptr;
    const GrowthConfig_t* m_pGrowthConfig = nullptr;
    PopCompositionCalculator* m_pCompositionCalculator = nullptr;
    int m_maxSize;
    int m_nutrientStockpile = 0;
    int m_compositionBatchDepth = 0;
    bool m_bCompositionDirty = false;

    RiotCalculator m_riot;
    GoldenAgeCalculator m_goldenAge;

    RiotConditionInputs BuildRiotInputs_() const;

    void NotifyPopGained_();
    void NotifyPopLost_();
    void MaybeRecalculateCompositionAfterSpecialistChange_();
    const std::string& GetDefaultPopType_() const;
};

} // namespace ac
