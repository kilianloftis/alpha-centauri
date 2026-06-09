#pragma once

#include "game/faction/population/PopContainer.h"
#include "game/faction/population/PopCompositionCalculator.h"
#include "game/faction/population/RiotCalculator.h"
#include "game/faction/population/GrowthCalculator.h"
#include "game/faction/population/GoldenAgeCalculator.h"

#include <memory>
#include <string>

namespace ac
{

struct RiotConditionInputs;

class PopTypeRegistry;

// PopulationManager is the API surface for the population component.
// It manages pop counts, composition, growth, and riot state for a single base,
// and provides hooks (signals) for external systems to react to population events.
class PopulationManager
{
public:
    PopulationManager();
    explicit PopulationManager(int initialSize);
    ~PopulationManager();

    // Population size management
    int GetSize() const;
    int GetGrowthRate() const;
    bool CanGrow() const;

    // Container access (for systems that need to iterate pops by stable ID)
    const PopContainer& GetContainer() const { return m_container; }

    // Pop access - delegated to PopContainer
    const std::vector<std::unique_ptr<Pop>>& GetPops() const { return m_container.GetPops(); }
    Pop* GetPop(size_t index) { return m_container.GetPop(index); }

    // Pop counts by type - delegated to PopContainer
    int GetWorkerCount() const { return m_container.GetWorkerCount(); }
    int GetTalentCount() const { return m_container.GetTalentCount(); }
    int GetDroneCount() const { return m_container.GetDroneCount(); }
    int GetSpecialistCount() const { return m_container.GetSpecialistCount(); }

    // Pop manipulation - no arguments, uses PopFactory internally
    void AddPop();
    void RemovePop();

    // Convert a pop to any type by config id (e.g. "Worker", "Drone", "Talent", "Librarian")
    void ConvertTo(size_t index, const std::string& typeId);

    // Drone and talent calculations
    bool IsRioting() const;
    bool IsDestroyed() const;

    // Add a drone (for faction base count mechanic) - delegated to PopContainer
    void AddDrone() { m_container.PromoteWorkerToDrone(); }

    // Reconcile actual pop composition against calculator targets.
    // Converts workers to drones/talents (or back) to match targetDrones/targetTalents.
    // No-op if no composition calculator is set.
    void RecalculateComposition();

    // Check riot conditions at end of turn. Delegates to m_riot.Update(inputs).
    void CheckRiotEndOfTurn();

    // Check golden age conditions at end of turn. Delegates to m_golden_age.Update(...).
    void CheckGoldenAgeEndOfTurn();

    // Advance growth accumulation by one turn.
    // nutrientsPerTurn is the net nutrient output of the base this turn.
    // Delegates to m_growth.Accumulate(); on_growth triggers AddPop, on_starvation triggers RemovePop.
    void AccumulateGrowth(int nutrientsPerTurn);

    // Registry injection — forwarded to PopContainer.
    // Fires on_pop_gained once per initial pop created from reserved capacity.
    void SetRegistry(const PopTypeRegistry* pRegistry);

    // Composition calculator injection
    void SetCompositionCalculator(PopCompositionCalculator* pCalculator);

    // Population limits
    int GetMaxSize() const;
    void SetMaxSize(int maxSize);

    // Current nutrients accumulated toward next growth.
    int GetNutrientBank() const;

    // Signals
    Signal<int> on_pop_gained;   // new size
    Signal<int> on_pop_lost;     // new size

    // Riot signals
    Signal<> on_will_riot;    // conditions met after growth, riot not yet active
    Signal<> on_is_rioting;   // end-of-turn: conditions still met, riot now active
    Signal<> on_riot_ended;   // end-of-turn: conditions no longer met, riot was active

    // Growth signals
    Signal<> on_growth;       // nutrient bank reached threshold; base may grow
    Signal<> on_starvation;   // nutrient bank went negative; base may shrink

    // Golden age signals
    Signal<> on_golden_age_started;
    Signal<> on_golden_age_ended;

private:
    PopContainer m_container;
    PopCompositionCalculator* m_pCompositionCalculator = nullptr;
    int m_maxSize;
    int m_growthRate;

    RiotCalculator m_riot;
    GrowthCalculator m_growth;
    GoldenAgeCalculator m_golden_age;

    const std::string& GetDefaultPopType_() const;
    RiotConditionInputs BuildRiotInputs_() const;

    void NotifyPopGained_();
    void NotifyPopLost_();
};

} // namespace ac
