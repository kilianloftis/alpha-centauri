#pragma once

#include "game/population/Pop.h"
#include "game/population/PopTypeConfigParser.h"
#include "lib/DerefView.h"
#include "lib/Revision.h"
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class PopTypeRegistry;

// Storage for a base's pops, counts and revision. The rules about types live in
// PopulationManager, so every conversion path applies them.
class PopContainer
{
public:
    PopContainer(const PopTypeRegistry& rRegistry, int initialSize);
    ~PopContainer() = default;

    int GetSize() const;
    // Iterate pops by reference without exposing the owning unique_ptrs.
    auto Pops() { return DerefView(m_pops); }
    auto Pops() const { return DerefView(m_pops); }

    // Remove a specific pop. Which pop dies is population *policy* and lives in
    // PopulationManager; the container only owns the storage.
    void Remove(const Pop& rPop);

    // GetWorkerCount includes drones and talents; GetPlainWorkerCount does not.
    int GetWorkerCount() const;
    int GetPlainWorkerCount() const;
    int GetTalentCount() const;
    int GetDroneCount() const;
    int GetSpecialistCount() const;

    // Bodies available to carry drone pressure and talents: every pop in the promotion graph.
    // This is the pool composition seats into, and the population the mood sums range over.
    int GetCompositionPoolCount() const;

    // Mood sums over the seated pops, both from one pass. Types outside the graph declare no
    // weights and so contribute 0 to each without needing to be filtered out.
    MoodWeights_t GetMoodWeightSums() const;

    // Container operations. AddPop takes an already-resolved type for the same reason ConvertTo
    // does: which types are legal is PopulationManager's decision, not the container's.
    void AddPop(const PopTypeConfig_t& rConfig);

    // Installs a type the caller already resolved (PopulationManager::ConvertTo).
    void ConvertTo(Pop& rPop, const PopTypeConfig_t& rConfig);

    // Compute total psych output across all pops
    int ComputePsychOutput() const;

    // Bumped on every pop mutation (add/remove/convert); consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    std::vector<std::unique_ptr<Pop>> m_pops;
    Revision m_revision;
    const PopTypeRegistry& m_rRegistry;

    int CountPops_(bool (*predicate)(const Pop*)) const;
};

} // namespace ac
