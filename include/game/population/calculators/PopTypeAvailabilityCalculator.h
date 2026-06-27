#pragma once

#include <string>
#include <vector>

namespace ac
{

class PopTypeRegistry;
struct PopTypeConfig_t;

class PopTypeAvailabilityCalculator
{
public:
    PopTypeAvailabilityCalculator(const PopTypeRegistry& rRegistry);
    ~PopTypeAvailabilityCalculator() = default;

    std::vector<const PopTypeConfig_t*> GetAvailable(const std::vector<std::string>& rDiscoveredTechs) const;

    // Resolves a type ID through the obsolescence chain given the currently discovered techs,
    // returning the most current non-obsoleted successor config.
    // Throws if the resolved type is not found in the registry.
    const PopTypeConfig_t& ResolveCurrentType(const std::string& rTypeId,
                                              const std::vector<std::string>& rDiscoveredTechs) const;

private:
    const PopTypeRegistry* m_pRegistry;
};

} // namespace ac
