#pragma once

#include <string>

namespace ac
{

class GameState;

// Whether a secret project can still be built. Two distinct questions, deliberately separate:
// a project that was built and later razed is *unavailable* forever (tombstoned) but is owned by
// nobody, and conflating the two mislabels a destroyed project as somebody's.
class SecretProjectAvailabilityCalculator
{
public:
    explicit SecretProjectAvailabilityCalculator(const GameState& rGameState);
    ~SecretProjectAvailabilityCalculator() = default;

    // Can no longer be built: some base holds it, or it was built and destroyed. This is the
    // availability question — what the build menu and AddBuilding need.
    bool IsUnavailable(const std::string& rBuildingId) const;

    // A base of some faction holds it *right now*. False for a destroyed project. This is the
    // ownership question — what a UI "owned by <faction>" label or a victory check needs.
    bool IsOwnedByAnyFaction(const std::string& rBuildingId) const;

private:
    const GameState& m_rGameState;
};

} // namespace ac
