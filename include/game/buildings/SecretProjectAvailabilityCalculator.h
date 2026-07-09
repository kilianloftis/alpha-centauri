#pragma once

#include <string>

namespace ac
{

class GameState;

// Determines whether a secret project building has already been completed
// by any faction. Iterates all bases of all factions to check their buildings.
class SecretProjectAvailabilityCalculator
{
public:
    explicit SecretProjectAvailabilityCalculator(const GameState& rGameState);
    ~SecretProjectAvailabilityCalculator() = default;

    // Returns true if the given building ID has been completed in any base of any faction.
    bool IsCompleted(const std::string& rBuildingId) const;

private:
    const GameState& m_rGameState;
};

} // namespace ac
