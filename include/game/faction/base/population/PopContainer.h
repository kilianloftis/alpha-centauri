#pragma once

#include "game/faction/base/population/pop-types/Pop.h"
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class PopTypeRegistry;
struct PopCompositionResult;

// Manages the population vector and pop transformations.
// Tracks assigned vs unassigned pops and handles automatic conversions.
class PopContainer
{
public:
    PopContainer();
    ~PopContainer() = default;

    // Container access
    int GetSize() const;
    const std::vector<std::unique_ptr<Pop>>& GetPops() const;
    Pop* GetPop(size_t index);

    // Population counts by type
    int GetWorkerCount() const;
    int GetTalentCount() const;
    int GetDroneCount() const;
    int GetSpecialistCount() const;

    // Container operations
    void Reserve(int count);  // Reserve capacity for pops (created when registry is set)
    void AddPop(const std::string& typeId);
    void RemovePop();

    // Convert a pop to any type by config id
    void ConvertTo(size_t index, const std::string& typeId);

    // Convert a worker to a drone (for faction base count mechanic)
    void PromoteWorkerToDrone();

    // Apply composition targets: converts excess drones/talents to workers,
    // then promotes workers to match targets.
    void ApplyCompositionTargets(const PopCompositionResult& targets, const std::string& defaultTypeId);

    // Registry injection. Returns the number of initial pops created from reserved capacity.
    int SetRegistry(const PopTypeRegistry* pRegistry);

    // Compute total psych output across all pops
    int ComputePsychOutput() const;

private:
    std::vector<std::unique_ptr<Pop>> m_pops;
    const PopTypeRegistry* m_pRegistry;
    int m_nextPopId;

    int CountPops_(bool (*predicate)(const Pop*)) const;
};

} // namespace ac
