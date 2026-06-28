#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ac
{

class Faction;

// Determines whether a secret project building has already been completed
// by any faction. Iterates all bases of all factions to check their buildings.
class SecretProjectAvailabilityCalculator
{
public:
    explicit SecretProjectAvailabilityCalculator(const std::vector<std::unique_ptr<Faction>>& rFactions);
    ~SecretProjectAvailabilityCalculator() = default;

    // Returns true if the given building ID has been completed in any base of any faction.
    bool IsCompleted(const std::string& rBuildingId) const;

private:
    const std::vector<std::unique_ptr<Faction>>* m_pFactions;
};

} // namespace ac
