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

// Storage for a base's pops: the vector, the counts, and the revision. Deliberately owns no
// population *policy* — which types a pop may become, how the obsolescence chain resolves, and
// how composition targets are reconciled all live in PopulationManager. Keeping the rules out
// of here is what makes them enforceable in one place: this class used to hold the availability
// calculator and apply it on one conversion path (ConvertToFallback) but not the other
// (ConvertTo), so ConvertTo could install a pop type the fallback path would have refused.
class PopContainer
{
public:
    // The registry is a reference: a null one used to construct a base with zero pops and then
    // throw on the first AddPop, which is a failure reported at the wrong place and time.
    PopContainer(const PopTypeRegistry& rRegistry, int initialSize);
    ~PopContainer() = default;

    // Container access
    int GetSize() const;
    // Iterate pops by reference without exposing the owning unique_ptrs.
    auto Pops() { return DerefView(m_pops); }
    auto Pops() const { return DerefView(m_pops); }

    // Population counts by type.
    // GetWorkerCount is every tile-capable pop — plain workers *and* drones *and* talents — so
    // it does not partition the population against the counts below. Callers that need the
    // disjoint "worker, not drone, not talent" bucket want GetPlainWorkerCount.
    int GetWorkerCount() const;
    int GetPlainWorkerCount() const;
    int GetTalentCount() const;
    int GetDroneCount() const;
    int GetSpecialistCount() const;

    // Container operations
    void AddPop(const std::string& typeId);
    void RemovePop();

    // Install an already-resolved type on a pop. The caller decides *which* type is legal —
    // see PopulationManager::ConvertTo, which resolves the id through the obsolescence chain
    // first, so every conversion path applies the same rule.
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
