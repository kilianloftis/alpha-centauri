#pragma once

#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/DerefView.h"
#include "lib/Revision.h"
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class PopTypeRegistry;
class PopTypeAvailabilityCalculator;
class ResearchManager;
struct PopCompositionResult;

// Manages the population vector and pop transformations.
// Tracks assigned vs unassigned pops and handles automatic conversions.
class PopContainer
{
public:
    PopContainer(const PopTypeRegistry* pReg,
                 const PopTypeAvailabilityCalculator* pAvailabilityCalculator,
                 const ResearchManager* pResearchManager,
                 int initialSize);
    ~PopContainer() = default;

    // Container access
    int GetSize() const;
    // Iterate pops by reference without exposing the owning unique_ptrs.
    auto Pops() { return DerefView(m_pops); }
    auto Pops() const { return DerefView(m_pops); }

    // Population counts by type
    int GetWorkerCount() const;
    int GetTalentCount() const;
    int GetDroneCount() const;
    int GetSpecialistCount() const;

    // Container operations
    void AddPop(const std::string& typeId);
    void RemovePop();

    // Convert a pop to any type by config id
    void ConvertTo(Pop& rPop, const std::string& typeId);

    // Convert this pop to its configured fallback type, resolved through the obsolescence chain
    // so the pop ends up as the most current non-obsoleted successor.
    // Throws if the pop has no fallback configured or the resolved type is not in the registry.
    void ConvertToFallback(Pop& rPop);

    // Apply composition targets: converts excess drones/talents to workers,
    // then promotes workers to match targets.
    void ApplyCompositionTargets(const PopCompositionResult& targets, const std::string& defaultTypeId);

    // Compute total psych output across all pops
    int ComputePsychOutput() const;

    // Bumped on every pop mutation (add/remove/convert); consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    std::vector<std::unique_ptr<Pop>> m_pops;
    Revision m_revision;
    const PopTypeRegistry* m_pRegistry;
    const PopTypeAvailabilityCalculator* m_pAvailabilityCalculator;
    const ResearchManager* m_pResearchManager;

    int CountPops_(bool (*predicate)(const Pop*)) const;
};

} // namespace ac
