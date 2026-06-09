#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ac
{

class Pop;
class PopTypeRegistry;

// PopManager handles population composition decisions
// For now, all new pops are workers. Talent/drone/specialist logic TBD.
class PopManager
{
public:
    PopManager();
    ~PopManager();

    // Inject the registry used to look up pop type definitions
    void SetRegistry(const PopTypeRegistry* pRegistry);

    // Create a pop of the given type id. Defaults to "Worker" if typeId is empty.
    // Returns nullptr if the id is not found in the registry.
    std::unique_ptr<Pop> CreatePop(const std::string& typeId = "Worker") const;

    // TODO: Add logic for determining pop type based on:
    // - Drone calculations (base size, psych output, faction modifiers)
    // - Talent calculations (psych output, faction modifiers)
    // - Specialist assignment (economy/labs/psych allocation)
    // - Random events (drone riots, etc.)

private:
    const PopTypeRegistry* m_pRegistry = nullptr;

    // TODO: Add state for population composition tracking
    // - Current worker count
    // - Current drone count
    // - Current talent count
    // - Current specialist count
    // - Psych output history
    // - Faction modifiers
};

} // namespace ac
