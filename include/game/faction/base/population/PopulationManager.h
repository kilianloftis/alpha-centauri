#pragma once

#include "game/faction/base/population/PopContainer.h"
#include "game/population/calculators/DroneCalculator.h"
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

// PopulationManager is the API surface for the population component.
// It manages pop counts, composition, growth, and riot state for a single base,
// and provides hooks (signals) for external systems to react to population events.
class PopulationManager
{
public:
    // References throughout: the growth config in particular used to be "optional" (falling
    // back to a hardcoded max size of 7, duplicating pop_growth.json's own default) while
    // GetNutrientsRequired and ApplyGrowth dereferenced it unchecked — so the supported null
    // case was a crash on the first growth turn. A null composition calculator likewise made
    // RecalculateComposition a silent no-op, which is the state fixtures used to build.
    PopulationManager(const PopTypeRegistry& rPopTypeRegistry,
                      const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
                      const GrowthConfig_t& rGrowthConfig,
                      DroneCalculator& rDroneCalculator,
                      PopCompositionCalculator& rCompositionCalculator,
                      const ResearchManager& rResearchManager,
                      int initialSize);
    ~PopulationManager();

    // Supplies faction- and base-scoped drone formula inputs. When unset, only base_size is
    // filled from the local population.
    void SetDroneInputSupplier(std::function<DroneInputs_t()> supplier);

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
    // Weighted drone count for riots (Super Drone = 2).
    int GetRiotContribution() const { return m_container.GetRiotContribution(); }
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
    void RecalculateComposition();

    // Reconcile against explicitly supplied targets. This is population *policy* — which pops
    // change and in what order — and lives here rather than in PopContainer, which is storage.
    // Every conversion it performs is resolved through the obsolescence chain, so the tech gate
    // applies here exactly as it does to ConvertTo and ConvertToFallback; the container's own
    // version applied it to neither.
    void ApplyCompositionTargets(const PopCompositionResult_t& rTargets,
                                 const std::string& droneTypeId,
                                 const std::string& talentTypeId);

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
    // rBaseEffects is this base's final effect list (BaseManager::BuildBaseEffects_).
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
    DroneCalculator& m_rDroneCalculator;
    PopCompositionCalculator& m_rCompositionCalculator;
    std::function<DroneInputs_t()> m_droneInputSupplier;
    int m_maxSize;
    int m_nutrientStockpile = 0;
    int m_compositionBatchDepth = 0;
    bool m_bCompositionDirty = false;
    std::function<int(const Pop&)> m_popValuator;

    RiotCalculator m_riot;
    GoldenAgeCalculator m_goldenAge;

    RiotConditionInputs_t BuildRiotInputs_() const;
    DroneInputs_t BuildDroneInputs_() const;

    void NotifyPopGained_();
    void NotifyPopLost_();
    void MaybeRecalculateComposition_();
    // Specialists last; within a group, the pop producing the least. See
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
