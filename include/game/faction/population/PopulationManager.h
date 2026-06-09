#pragma once

#include "game/faction/population/Pop.h"
#include "game/faction/population/PopFactory.h"
#include "game/faction/population/PopCompositionCalculator.h"
#include "game/faction/population/RiotCalculator.h"
#include "game/faction/population/GrowthCalculator.h"
#include "game/faction/population/GoldenAgeCalculator.h"
#include <memory>
#include <string>
#include <vector>

namespace ac
{

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
    void SetSize(int size);
    int GetGrowthRate() const;
    bool CanGrow() const;

    // Pop access
    const std::vector<std::unique_ptr<Pop>>& GetPops() const;
    Pop* GetPop(size_t index);

    // Pop counts by type
    int GetWorkerCount() const;
    int GetTalentCount() const;
    int GetDroneCount() const;
    int GetSpecialistCount() const;

    // Pop manipulation - no arguments, uses PopFactory internally
    void AddPop();
    void RemovePop();

    // Convert a pop to any type by config id (e.g. "Worker", "Drone", "Talent", "Librarian")
    void ConvertTo(size_t index, const std::string& typeId);

    // Drone and talent calculations
    bool HasDroneRiot() const;
    bool IsDestroyed() const;

    // Add a drone (for faction base count mechanic)
    void AddDrone();

    // Reconcile actual pop composition against calculator targets.
    // Converts workers to drones/talents (or back) to match targetDrones/targetTalents.
    // No-op if no composition calculator is set.
    void RecalculateComposition();

    // Check riot conditions at end of turn. Delegates to m_riot.Update(HasDroneRiot()).
    void CheckRiotEndOfTurn();

    // Check golden age conditions at end of turn. Delegates to m_golden_age.Update(...).
    void CheckGoldenAgeEndOfTurn();

    // Advance growth accumulation by one turn.
    // nutrientsPerTurn is the net nutrient output of the base this turn.
    // Delegates to m_growth.Accumulate(); on_growth triggers AddPop, on_starvation triggers RemovePop.
    void AccumulateGrowth(int nutrientsPerTurn);

    // Registry injection — forwarded to the internal PopFactory
    void SetRegistry(const PopTypeRegistry* pRegistry);

    // Composition calculator injection
    void SetCompositionCalculator(PopCompositionCalculator* pCalculator);

    // Population limits
    int GetMaxSize() const;
    void SetMaxSize(int maxSize);

    // Accessors for calculator subsystems (const ref for signal subscription and state queries)
    const RiotCalculator& GetRiot() const;
    const GrowthCalculator& GetGrowth() const;
    const GoldenAgeCalculator& GetGoldenAge() const;

    // Signals
    Signal<int> on_pop_gained;   // new size
    Signal<int> on_pop_lost;     // new size

private:
    std::vector<std::unique_ptr<Pop>> m_pops;
    std::unique_ptr<PopFactory> m_pPopFactory;
    PopCompositionCalculator* m_pCompositionCalculator = nullptr;
    int m_maxSize;
    int m_growthRate;

    RiotCalculator m_riot;
    GrowthCalculator m_growth;
    GoldenAgeCalculator m_golden_age;

    int CountPops_(bool (*predicate)(const Pop*)) const;
    int ComputePsychOutput_() const;
    const std::string& GetDefaultPopType_() const;

    void NotifyPopGained_();
    void NotifyPopLost_();
};

} // namespace ac
