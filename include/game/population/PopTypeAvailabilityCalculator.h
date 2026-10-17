#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

class PopTypeRegistry;
struct PopTypeConfig_t;

// Answers "which specialist types can the player assign?" and "what has this type become?"
// from one walk of the `obsoletes` graph, so the two cannot disagree.
//
// Obsolescence is transitive and does NOT gate intermediate steps on their own tech: if
// Transcend obsoletes Empath and Empath obsoletes Doctor, then Transcend obsoletes Doctor,
// whether or not Empath was ever researched.
class PopTypeAvailabilityCalculator
{
public:
    PopTypeAvailabilityCalculator(const PopTypeRegistry& rRegistry);
    ~PopTypeAvailabilityCalculator() = default;

    // Types the player may assign: player_assignable, tech discovered, and not superseded —
    // that last clause being exactly "ResolveCurrentType returns the type itself".
    std::vector<const PopTypeConfig_t*> GetAvailable(const std::vector<std::string>& rDiscoveredTechs) const;

    // What rTypeId has become: the deepest available successor along its obsolescence chain,
    // or the type itself when nothing available supersedes it. Between equally deep available
    // successors the earliest in registry order wins; a deeper successor beats a shallower one
    // even when the shallower is also available.
    // Throws if the resolved id is not in the registry. Cycles are rejected at load, by
    // PopTypeRegistry, so they cannot be reached here.
    const PopTypeConfig_t& ResolveCurrentType(const std::string& rTypeId,
                                              const std::vector<std::string>& rDiscoveredTechs) const;

private:
    // Id of the deepest available successor of rTypeId, or rTypeId itself. The single
    // definition of the obsolescence rule; both public methods are phrased in terms of it.
    // Takes the discovered set already built, so GetAvailable builds it once for all candidates.
    std::string ResolveSuccessorId_(const std::string& rTypeId,
                                    const std::unordered_set<std::string>& rDiscovered) const;

    const PopTypeRegistry& m_rRegistry;
};

} // namespace ac
