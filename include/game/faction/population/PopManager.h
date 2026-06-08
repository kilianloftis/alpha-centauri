#pragma once

#include <memory>
#include <vector>

namespace ac
{

class Pop;

// PopManager handles population composition decisions
// For now, all new pops are workers. Talent/drone/specialist logic TBD.
class PopManager
{
public:
    PopManager();
    ~PopManager();

    // Create a new pop with appropriate type
    // Currently always returns a WorkerPop
    std::unique_ptr<Pop> CreatePop();

    // TODO: Add logic for determining pop type based on:
    // - Drone calculations (base size, psych output, faction modifiers)
    // - Talent calculations (psych output, faction modifiers)
    // - Specialist assignment (economy/labs/psych allocation)
    // - Random events (drone riots, etc.)

private:
    // TODO: Add state for population composition tracking
    // - Current worker count
    // - Current drone count
    // - Current talent count
    // - Current specialist count
    // - Psych output history
    // - Faction modifiers
};

} // namespace ac
