#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

class PopTypeRegistry;
struct PopTypeConfig_t;

// Assignable specialist types and what a type has become, from one walk of the `obsoletes`
// graph. Obsolescence is transitive, whatever intermediate steps were researched.
class PopTypeAvailabilityCalculator
{
public:
    PopTypeAvailabilityCalculator(const PopTypeRegistry& rRegistry);
    ~PopTypeAvailabilityCalculator() = default;

    // Types the player may assign: player_assignable, tech discovered, and not superseded —
    // that last clause being exactly "ResolveCurrentType returns the type itself".
    std::vector<const PopTypeConfig_t*> GetAvailable(const std::vector<std::string>& rDiscoveredTechs) const;

    // The deepest available successor of rTypeId, or itself; ties go to registry order. Throws
    // if the id is unknown.
    const PopTypeConfig_t& ResolveCurrentType(const std::string& rTypeId,
                                              const std::vector<std::string>& rDiscoveredTechs) const;

private:
    // The one definition of the obsolescence rule, over a discovered set built once.
    std::string ResolveSuccessorId_(const std::string& rTypeId,
                                    const std::unordered_set<std::string>& rDiscovered) const;

    const PopTypeRegistry& m_rRegistry;
};

} // namespace ac
